/*
   HeartThru with Bluetooth Audio

   Created: Chip Audette, OpenAudio, March 2019
   Purpose: Designed or use with OpenTact headsets and Tympan electronics.

   Features:
    * Electronic hear-thru with multiple algorithms
    * Remote control via Android App
    * Received audio over bluetooth

   MIT License.  use at your own risk.
*/


// Include all the of the needed libraries
#include <Tympan_Library.h>
#include <SparkFunbc127.h>  //from https://github.com/sparkfun/SparkFun_BC127_Bluetooth_Module_Arduino_Library

// State constants
const int AUDIO_MUTE=0, AUDIO_MONO=1, AUDIO_STEREO=2;
const int ALG_LINEAR=0, ALG_FASTCOMP=1, ALG_SLOWCOMP=2;
const int INPUT_PCBMICS=0, INPUT_MICJACK=1,INPUT_LINEIN_SE=2;


//local files
#include "SDAudioWriter.h" 
#include "SerialManager.h"

//definitions for SD writing
#define MAX_F32_BLOCKS (192)      //Can't seem to use more than 192, so you could set it to 192.  Won't run at all if much above 400.  


//set the sample rate and block size
//const float sample_rate_Hz = 44117.0f ; //24000 or 44117 (or other frequencies in the table in AudioOutputI2S_F32)
const float sample_rate_Hz = 96000.0f ; //24000 or 44117 (or other frequencies in the table in AudioOutputI2S_F32)
const int audio_block_samples = 128;     //do not make bigger than AUDIO_BLOCK_SAMPLES from AudioStream.h (which is 128)  Must be 128 for SD recording.
AudioSettings_F32 audio_settings(sample_rate_Hz, audio_block_samples);

// Define the overall setup
String overall_name = String("Tympan: Multi-Mode Hear-Thru wBTAudio");
float default_input_gain_dB = 5.0f; //gain on the microphone
float input_gain_dB = default_input_gain_dB;
float vol_knob_gain_dB = 0.0; //will be overridden by volume knob, if used
float output_volume_dB = 0.0; //volume setting of output PGA

// Sparkfun Class for enabling BT Audio
BC127 BTModu(&Serial1); //which serial is connected to the BT module? Serial1 is.

// /////////// Define audio objects...they are configured later

//create audio library objects for handling the audio
Tympan                        myTympan(TympanRev::D);
AudioInputI2S_F32             i2s_in(audio_settings);   //Digital audio input from the ADC
//AudioRecordQueue_F32          queueL(audio_settings),       queueR(audio_settings);     //gives access to audio data (will use for SD card)
SDAudioWriter_F32asI16        stereoSDWriter(audio_settings);
AudioMixer4_F32               inputMixerL(audio_settings),  inputMixerR(audio_settings);
//AudioFilterBiquad_F32         iirLeft(audio_settings),      iirRight(audio_settings);           //xy=233,172
AudioSwitch4_F32              inputSwitchL(audio_settings), inputSwitchR(audio_settings); //for switching between the algorithms
AudioEffectCompWDRC_F32       fastCompL(audio_settings),    fastCompR(audio_settings);  // fast compression
AudioEffectCompWDRC_F32       slowCompL(audio_settings),    slowCompR(audio_settings);  // slow compression
AudioMixer4_F32               outputMixerL(audio_settings), outputMixerR(audio_settings);  // for mixing together the diff algorithms
AudioOutputI2S_F32            i2s_out(audio_settings);  //Digital audio output to the DAC.  Should always be last.
  
//Connect Left & Right Input Channel to Left and Right SD card queue
AudioConnection_F32           patchcord1(i2s_in, 0, stereoSDWriter, 0);   //connect Raw audio to left channel of SD writer
AudioConnection_F32           patchcord2(i2s_in, 1, stereoSDWriter, 1);  //connect Raw audio to right channel of SD writer

//AUDIO CONNECTIONS...start with inputs
AudioConnection_F32           patchcord3(i2s_in, 0, inputMixerL, 0);     //Left audio to Left mixer 
AudioConnection_F32           patchcord4(i2s_in, 1, inputMixerL, 1);     //Right audio to Left mixer 
AudioConnection_F32           patchcord5(i2s_in, 0, inputMixerR, 0);     //Left audio to Right mixer
AudioConnection_F32           patchcord6(i2s_in, 1, inputMixerR, 1);     //Right audio to Right mixer
//AudioConnection_F32           patchcord7(inputMixerL, 0, iirLeft, 0);   //connect left audio to IIR filter
//AudioConnection_F32           patchcord8(inputMixerR, 0, iirRight, 0);  //connect right audio to IIR filter
//AudioConnection_F32           patchcord9(iirLeft, 0, inputSwitchL, 0);   //connect IIR filter to left switch
//AudioConnection_F32           patchcord10(iirRight, 0, inputSwitchR, 0);  //connect IIR filter audio to right switch
AudioConnection_F32           patchcord7(inputMixerL, 0, inputSwitchL, 0);   //connect left audio to switch
AudioConnection_F32           patchcord8(inputMixerR, 0, inputSwitchR, 0);  //connect right audio to switch

// Connections for Linear
AudioConnection_F32           patchcord100(inputSwitchL,ALG_LINEAR,outputMixerL,ALG_LINEAR); //pass the left signal through to the left output mixer
AudioConnection_F32           patchcord101(inputSwitchR,ALG_LINEAR,outputMixerR,ALG_LINEAR); //pass the right signal through to the right output mixer

//Connections for Fast Compression
AudioConnection_F32           patchcord200(inputSwitchL,ALG_FASTCOMP,fastCompL,0); //pass the left signal to left compressor
AudioConnection_F32           patchcord201(inputSwitchR,ALG_FASTCOMP,fastCompR,0); //pass the right signal to right compressor
AudioConnection_F32           patchcord202(fastCompL,0,outputMixerL,ALG_FASTCOMP); //pass the left signal to left compressor
AudioConnection_F32           patchcord203(fastCompR,0,outputMixerR,ALG_FASTCOMP); //pass the right signal to right compressor

//Connections for Slow Compression
AudioConnection_F32           patchcord300(inputSwitchL,ALG_SLOWCOMP,slowCompL,0); //pass the left signal to left compressor
AudioConnection_F32           patchcord301(inputSwitchR,ALG_SLOWCOMP,slowCompR,0); //pass the right signal to right compressor
AudioConnection_F32           patchcord302(slowCompL,0,outputMixerL,ALG_SLOWCOMP); //pass the left signal to left compressor
AudioConnection_F32           patchcord303(slowCompR,0,outputMixerR,ALG_SLOWCOMP); //pass the right signal to right compressor

//Connect to outputs
AudioConnection_F32           patchcord500(outputMixerL, 0, i2s_out, 0);    //Left mixer to left output
AudioConnection_F32           patchcord501(outputMixerR, 0, i2s_out, 1);    //Right mixer to right output

void setAlgorithmParameters(void) {
  { 
    //configure linear ... nothing to set!
  }

  //universal compression parameters
  float maxdB = 115.0;  //calibration factor.  What dB SPL is full scale?  For OpenTact at +5dB mic gain: 115dB in room results in FS.
  float exp_cr = 1.0, exp_end_knee = 0.0;    //expansion regime: compression ratio and knee point.  1.0 defeats this feature
  float tkgain = 0.0;     //compression-start gain (ie, gain of linear regime).

  {
    //configure fast compression
    float attack_ms = 5.0, release_ms = 100.0;
    float comp_ratio = 5.0; //compression regime: compression ratio
    float tk = 85.0;        //compression regime: compression knee point (dB SPL...related via maxdB)
    float bolt = 105.0;     //compression regime: output limiter
    
    fastCompL.setParams(attack_ms, release_ms, maxdB, exp_cr, exp_end_knee, tkgain, comp_ratio, tk, bolt);
    fastCompR.setParams(attack_ms, release_ms, maxdB, exp_cr, exp_end_knee, tkgain, comp_ratio, tk, bolt);   
  }
  {
    //configure slow compression
    float attack_ms = 3000.0, release_ms = 3000.0;
    float comp_ratio = 5.0; //compression regime: compression ratio
    float tk = 80.0;        //compression regime: compression knee point (dB SPL...related via maxdB)
    float bolt = 110.0;     //compression regime: output limiter
    
    slowCompL.setParams(attack_ms, release_ms, maxdB, exp_cr, exp_end_knee, tkgain, comp_ratio, tk, bolt);
    slowCompR.setParams(attack_ms, release_ms, maxdB, exp_cr, exp_end_knee, tkgain, comp_ratio, tk, bolt);   
  }
}

//control display and serial interaction
bool enable_printCPUandMemory = false;
void togglePrintMemoryAndCPU(void) { enable_printCPUandMemory = !enable_printCPUandMemory; }; //"extern" let's be it accessible outside
void setPrintMemoryAndCPU(bool state) { enable_printCPUandMemory = state; };
bool enable_printAveSignalLevels = false;
bool printAveSignalLevels_as_dBSPL = false;
void togglePrintAveSignalLevels(bool as_dBSPL) { enable_printAveSignalLevels = !enable_printAveSignalLevels; printAveSignalLevels_as_dBSPL = as_dBSPL;};
SerialManager serialManager(myTympan);
#define BOTH_SERIAL myTympan

//keep track of state
State_t myState;

int current_config = 0;
void setConfiguration(int config) { 
  
  myTympan.volume_dB(-60.0); delay(50);  //mute the output audio
  myState.input_source = config;
  
  switch (config) {
    case INPUT_PCBMICS:
      //Select Input
      myTympan.inputSelect(TYMPAN_INPUT_ON_BOARD_MIC); // use the on-board microphones

      //Set input gain to 0dB
      input_gain_dB = 15;
      myTympan.setInputGain_dB(input_gain_dB);

      //Store configuration
      current_config = INPUT_PCBMICS;
      break;
      
    case INPUT_MICJACK:
      //Select Input
      myTympan.inputSelect(TYMPAN_INPUT_JACK_AS_MIC); // use the mic jack
      myTympan.setEnableStereoExtMicBias(true);

      //Set input gain to 0dB
      input_gain_dB = default_input_gain_dB;
      myTympan.setInputGain_dB(input_gain_dB);

      //Store configuration
      current_config = INPUT_MICJACK;
      break;
      
    case INPUT_LINEIN_SE:
      //Select Input
      myTympan.inputSelect(TYMPAN_INPUT_LINE_IN); // use the line-input through holes

      //Set input gain to 0dB
      input_gain_dB = default_input_gain_dB;
      myTympan.setInputGain_dB(input_gain_dB);

      //Store configuration
      current_config = INPUT_LINEIN_SE;
      break;
  }

  //bring the output volume back up
  delay(50);  myTympan.volume_dB(output_volume_dB);  // output amp: -63.6 to +24 dB in 0.5dB steps.  uses signed 8-bit
}

// ///////////////// Main setup() and loop() as required for all Arduino programs

// define the setup() function, the function that is called once when the device is booting
void setup() {
  delay(100);
  
  myTympan.beginBothSerial(); delay(1000);
  BOTH_SERIAL.print(overall_name);BOTH_SERIAL.println(": setup():...");
  BOTH_SERIAL.print("Sample Rate (Hz): "); BOTH_SERIAL.println(audio_settings.sample_rate_Hz);
  BOTH_SERIAL.print("Audio Block Size (samples): "); BOTH_SERIAL.println(audio_settings.audio_block_samples);

  //allocate the audio memory
  AudioMemory_F32_wSettings(MAX_F32_BLOCKS,audio_settings); //I can only seem to allocate 400 blocks
  BOTH_SERIAL.println("Setup: memory allocated.");
  
  //activate the Tympan audio hardware
  myTympan.enable();        // activate AIC
  
  //temporariliy mute the system
  myTympan.volume_dB(-60.0);  // output amp: -63.6 to +24 dB in 0.5dB steps.  uses signed 8-bit
  float cutoff_Hz = 70.0;  //set the default cutoff frequency for the highpass filter
  myTympan.setHPFonADC(true, cutoff_Hz, audio_settings.sample_rate_Hz); //set to false to disble

  //setup the audio processing
  setAlgorithmParameters();
  setAudioStereo();
  setAudioLinear();

  //Configure for Tympan PCB mics
  BOTH_SERIAL.println("Setup: Using Mic Jack with Mic Bias.");
  setConfiguration(INPUT_MICJACK); //this will also unmute the system
 
  //update the potentiometer settings
	//servicePotentiometer(millis());

  //service BT Audio
  serviceBTAudio();

  //Set the Bluetooth audio to go straight to the headphone amp, not through the Tympan software
  myTympan.mixBTAudioWithOutput(true);

  //prepare serial comms for SD messages to user
 stereoSDWriter.setSerial(&myTympan);
 

  //End of setup
  BOTH_SERIAL.println("Setup: complete.");serialManager.printHelp();

} //end setup()

// define the loop() function, the function that is repeated over and over for the life of the device


void loop() {
  //asm(" WFI");  //save power by sleeping.  Wakes when an interrupt is fired (usually by the audio subsystem...so every 256 audio samples)

  //respond to Serial commands
  while (Serial.available()) serialManager.respondToByte((char)Serial.read());   //USB Serial
  while (Serial1.available()) serialManager.respondToByte((char)Serial1.read()); //BT Serial
  
  //service the SD recording
  serviceSD();

  //update the memory and CPU usage...if enough time has passed
  if (enable_printCPUandMemory) printCPUandMemory(millis());

  serviceLEDs();
  
  //print info about the signal processing
  //updateAveSignalLevels(millis());
  //if (enable_printAveSignalLevels) printAveSignalLevels(millis(),printAveSignalLevels_as_dBSPL);

} //end loop()



// ///////////////// Servicing routines

void serviceBTAudio(void) {
  // resetFlag tracks the state of the module. When true, the module is reset and
  //  ready to receive a connection. When false, the module either has an active
  //  connection or has lost its connection; checking connectionState() will verify
  //  which of those conditions we're in.
  static boolean resetFlag = false;

  // This is where we determine the state of the module. If resetFlag is false, and
  //  we have a CONNECT_ERROR, we need to restart the module to clear its pairing
  //  list so it can accept another connection.
  if (BTModu.connectionState() == BC127::CONNECT_ERROR  && !resetFlag)
  {  
    Serial.println("Connection lost! Resetting...");
    // Blast the existing settings of the BC127 module, so I know that the module is
    //  set to factory defaults...
    BTModu.restore();

    // ...but, before I restart, I need to set the device to be a SINK, so it will
    //  enable its audio output and dump the audio to the output.
    BTModu.setClassicSink();

    // Write, reset, to commit and effect the change to a source.
    BTModu.writeConfig();
    // Repeat the connection process if we've lost our connection.
    BTModu.reset();
    // Change the resetFlag, so we know we've restored the module to a usable state.
    resetFlag = true;
  }
  // If we ARE connected, we'll issue the "PLAY" command. Note that issuing this when
  //  we are already playing doesn't hurt anything.
  else {
    //BTModu.musicCommands(BC127::PLAY);
  }

  // We want to look for a connection to be made to the module; once a connection
  //  has been made, we can clear the resetFlag.
  if (BTModu.connectionState() == BC127::SUCCESS) resetFlag = false;

}


void printCPUandMemory(unsigned long curTime_millis) {
  static unsigned long updatePeriod_millis = 3000; //how many milliseconds between updating gain reading?
  static unsigned long lastUpdate_millis = 0;

  //has enough time passed to update everything?
  if (curTime_millis < lastUpdate_millis) lastUpdate_millis = 0; //handle wrap-around of the clock
  if ((curTime_millis - lastUpdate_millis) > updatePeriod_millis) { //is it time to update the user interface?
    printCPUandMemoryMessage();  
    lastUpdate_millis = curTime_millis; //we will use this value the next time around.
  }
}
void printCPUandMemoryMessage(void) {
    BOTH_SERIAL.print("CPU Cur/Pk: ");
    BOTH_SERIAL.print(audio_settings.processorUsage(),1);
    BOTH_SERIAL.print("%/");
    BOTH_SERIAL.print(audio_settings.processorUsageMax(),1);
    BOTH_SERIAL.print("%, ");
    BOTH_SERIAL.print("MEM Cur/Pk: ");
    BOTH_SERIAL.print(AudioMemoryUsage_F32());
    BOTH_SERIAL.print("/");
    BOTH_SERIAL.print(AudioMemoryUsageMax_F32());
    BOTH_SERIAL.println();
}

void serviceLEDs(void) {
  if (stereoSDWriter.getState() == SDAudioWriter::STATE::UNPREPARED) {
    myTympan.setRedLED(HIGH); myTympan.setAmberLED(HIGH); //Turn ON both
  } else if (stereoSDWriter.getState() == SDAudioWriter::STATE::RECORDING) {
    myTympan.setRedLED(LOW); myTympan.setAmberLED(HIGH); //Go Amber
  } else {
    myTympan.setRedLED(HIGH); myTympan.setAmberLED(LOW); //Go Red
  }
}

void serviceSD(void) {
  if (stereoSDWriter.isFileOpen()) {
    AudioRecordQueue_F32 *queueL = &stereoSDWriter.queueL, *queueR = &stereoSDWriter.queueR;
    
    //if audio data is ready, write it to SD
    if ((queueL->available()) && (queueR->available())) {
      audio_block_f32_t *left = queueL->getAudioBlock(), *right = queueR->getAudioBlock();
      stereoSDWriter.interleaveAndWrite(
              left->data,    //float32 array for left audio channel
              right->data,   //float32 array for right audio channel
              left->length); //number of samples in each channel
      queueL->freeBuffer(); queueR->freeBuffer();  //free up these blocks now that they are written

      //print a warning if there has been an SD writing hiccup
      if (PRINT_OVERRUN_WARNING) {
        if (queueL->getOverrun() || queueR->getOverrun() || i2s_in.get_isOutOfMemory()) {
          float blocksPerSecond = (left->fs_Hz) / ((float)(left->length));
          BOTH_SERIAL.print("SD Write Warning: there was a hiccup in the writing.  Approx Time (sec): ");
          BOTH_SERIAL.println( ((float)stereoSDWriter.getNBlocksWritten()) / blocksPerSecond );
        }
      }

      //print timing information to help debug hiccups in the audio.  Are the writes fast enough?  Are there overruns?
      if (PRINT_FULL_SD_TIMING) {
        Serial.print("SD Write Status: "); 
        Serial.print(queueL->getOverrun()); //zero means no overrun
        Serial.print(", ");
        Serial.print(queueR->getOverrun()); //zero means no overrun
        Serial.print(", ");
        Serial.print(AudioMemoryUsageMax_F32());  //hopefully, is less than MAX_F32_BLOCKS
        //Serial.print(", ");
        //Serial.print(MAX_F32_BLOCKS);  // max possible memory allocation
        Serial.print(", ");
        Serial.println(i2s_in.get_isOutOfMemory());  //zero means i2s_input always had memory to work with.  Non-zero means it ran out at least once.
        
        //Now that we've read the flags, reset them.
        AudioMemoryUsageMaxReset_F32();
      }

      queueL->clearOverrun();
      queueR->clearOverrun();
      i2s_in.clear_isOutOfMemory();
    }
  } else {
    //no SD recording currently, so no SD action
  }
}


// //////////////////////////////////// Control the audio processing from the SerialManager
//here's a function to change the volume settings.   We'll also invoke it from our serialManager
void incrementInputGain(float increment_dB) {
  input_gain_dB += increment_dB;
  if (input_gain_dB < 0.0) {
    BOTH_SERIAL.println("Error: cannot set input gain less than 0 dB.");
    BOTH_SERIAL.println("Setting input gain to 0 dB.");
    input_gain_dB = 0.0;
  }
  myTympan.setInputGain_dB(input_gain_dB);
}

void setAudioMute(void) {
  myState.audio = AUDIO_MUTE;
  inputMixerL.gain(0, 0.0);      inputMixerL.gain(1, 0.0); //mute left and right input to left side
  inputMixerR.gain(0, 0.0);      inputMixerR.gain(1, 0.0); //mute left and right input to the right side
}
void setAudioMono(void) {
  myState.audio = AUDIO_MONO;
  inputMixerL.gain(0, 0.5);      inputMixerL.gain(1, 0.5);  //50% left and 50% right to the left side
  inputMixerR.gain(0, 0.5);      inputMixerR.gain(1, 0.5);  //50% left and 50% right to the right side
}
void setAudioStereo(void) {
  myState.audio = AUDIO_STEREO;
  inputMixerL.gain(0, 1.0);      inputMixerL.gain(1, 0.0); //100% left to left side
  inputMixerR.gain(0, 0.0);      inputMixerR.gain(1, 1.0); //100% right to the right side
}
void setAudioLinear(void) {
  myState.alg = ALG_LINEAR;
  inputSwitchL.setChannel(ALG_LINEAR);  inputSwitchR.setChannel(ALG_LINEAR);
}
void setAudioFastComp(void) {
  myState.alg = ALG_FASTCOMP;
  inputSwitchL.setChannel(ALG_FASTCOMP);  inputSwitchR.setChannel(ALG_FASTCOMP);
}
void setAudioSlowComp(void) {
  myState.alg = ALG_SLOWCOMP;
  inputSwitchL.setChannel(ALG_SLOWCOMP);  inputSwitchR.setChannel(ALG_SLOWCOMP);
}

void scaleCompressionSpeed(float scale_value, bool print_new_vals) {
  switch (myState.alg) {
    case ALG_LINEAR:
      BOTH_SERIAL.println("LINEAR: Ignoring change command.");
      break;
   case ALG_FASTCOMP:
      incrementAttackRelease(scale_value,fastCompL,fastCompR,print_new_vals);
      //printAttackRelease(fastCompL);
      break;
   case ALG_SLOWCOMP:
      incrementAttackRelease(scale_value,slowCompL,slowCompR,print_new_vals);
      //printAttackRelease(slowCompL);
  }
}
void incrementAttackRelease(float scale_value,AudioEffectCompWDRC_F32 &compL, AudioEffectCompWDRC_F32 &compR,bool print_new_vals) {
  float attack_msec = compL.getAttack_msec();
  float release_msec = compL.getRelease_msec();
  float min_attack_msec = 2.0, min_release_msec = 50;
  attack_msec = max(min_attack_msec,attack_msec*scale_value);
  release_msec = max(min_release_msec,release_msec*scale_value);
  compL.setAttackRelease_msec(attack_msec,release_msec);
  compR.setAttackRelease_msec(attack_msec,release_msec);
  if (print_new_vals) {
    BOTH_SERIAL.print("Comp: "); BOTH_SERIAL.print((int)attack_msec); BOTH_SERIAL.print("ms attack, ");
    BOTH_SERIAL.print((int)release_msec); BOTH_SERIAL.println("ms release");
  }
}
void incrementKneepoint(float increment_dB,bool print_new_vals) {
  switch (myState.alg) {
    case ALG_LINEAR:
      BOTH_SERIAL.println("LINEAR: Ignoring change command.");
      break;
   case ALG_FASTCOMP:
      incrementKneepoint(increment_dB,fastCompL,fastCompR,print_new_vals);
      break;
   case ALG_SLOWCOMP:
      incrementKneepoint(increment_dB,slowCompL,slowCompR,print_new_vals);
  }
}
void incrementKneepoint(float increment_dB,AudioEffectCompWDRC_F32 &compL, AudioEffectCompWDRC_F32 &compR,bool print_new_vals) {
  float knee_dB = compL.getKneeCompressor_dBSPL();
  float min_knee_dB = 0.0;
  knee_dB = max(min_knee_dB,knee_dB+increment_dB);
  compL.setKneeCompressor_dBSPL(knee_dB);
  compR.setKneeCompressor_dBSPL(knee_dB);
  if (print_new_vals) {
    BOTH_SERIAL.print("Comp: Kneepoint set to "); BOTH_SERIAL.print((int)knee_dB); BOTH_SERIAL.println("dB");
  }
}

