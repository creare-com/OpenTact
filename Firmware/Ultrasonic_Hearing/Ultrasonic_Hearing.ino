/*
  Tympan_Ultrasound

  Created: Chip Audette (OpenAudio), Feb 2017

  Purpose: Sample fast enough (96 kHz) to acquire ultrasound signals (up to 48 kHz)
      then do single side-band shifting of the signals down to the audible range

  Uses Teensy Audio Adapter ro the Tympan Audio Board
      For Teensy Audio Board, assumes microphones (or whatever) are attached to the
      For Tympan Audio Board, can use on board mics or external mic

  User Controls:
    Potentiometer on Tympan controls the amount of frequency shift

   MIT License.  use at your own risk.
*/

//do stereo processing?
#define USE_STEREO 1

//definitions for SD writing
#define MAX_F32_BLOCKS (192)      //Can't seem to use more than 192, so you could set it to 192.  Won't run at all if much above 400.  

// Include all the of the needed libraries
#include <Tympan_Library.h> //for AudioConvert_I16toF32, AudioConvert_F32toI16, and AudioEffectGain_F32
#include "SDAudioWriter.h"
#include "SerialManager.h"

const float sample_rate_Hz = 96000.0f ; //24000 or 44117.64706f (or other frequencies in the table in AudioOutputI2S_F32
const int audio_block_samples = 128;  //do not make bigger than AUDIO_BLOCK_SAMPLES from AudioStream.h
AudioSettings_F32   audio_settings(sample_rate_Hz, audio_block_samples);

// Define the overall setup
String overall_name = String("Tympan: Ultrasound Listening");
float default_input_gain_dB = 5.0f; //gain on the microphone
float input_gain_dB = default_input_gain_dB;
float vol_knob_gain_dB = 0.0; //will be overridden by volume knob
float ultrasound_gain_dB = 5.0;

// /////////// Define audio objects...they are configured later

//create audio library objects for handling the audio
TympanPins                  tympPins(TYMPAN_REV_D3); //TYMPAN_REV_C or TYMPAN_REV_D
TympanBase                  audioHardware(tympPins);
AudioInputI2S_F32           i2s_in(audio_settings);  //Digital audio *from* the Teensy Audio Board ADC.  Sends Int16.  Stereo.
AudioSynthWaveformSine_F32  carrier(audio_settings);   
AudioRecordQueue_F32        queueL(audio_settings), queueR(audio_settings);     //gives access to audio data (will use for SD card)

AudioEffectGain_F32         preGainL(audio_settings), preGainR(audio_settings);   
AudioFilterBiquad_F32       iirL1(audio_settings), iirL2(audio_settings), iirR1(audio_settings), iirR2(audio_settings);         
AudioMathMultiply_F32       multiplyL(audio_settings), multiplyR(audio_settings);  
AudioMixer4_F32             mixerL(audio_settings), mixerR(audio_settings);
AudioEffectCompWDRC_F32     fastCompL(audio_settings), fastCompR(audio_settings);      
AudioOutputI2S_F32          i2s_out(audio_settings);        //Digital audio *to* the Teensy Audio Board DAC.  Expects Int16.  Stereo

//Connect Left & Right Input Channel to Left and Right SD card queue
AudioConnection_F32           patchcord1001(i2s_in, 0, queueL, 0);   //connect Raw audio to queue (to enable SD writing)
AudioConnection_F32           patchcord1002(i2s_in, 1, queueR, 0);  //connect Raw audio to queue (to enable SD writing)

//Make all of the audio connections for the left audio channel
AudioConnection_F32         patchCord20(i2s_in, 0, mixerL, 0);  //raw path for hear-thru
AudioConnection_F32         patchCord1(i2s_in, 0, preGainL, 0); //processed path for ultrasound
AudioConnection_F32         patchCord2(preGainL, 0, iirL1, 0);
AudioConnection_F32         patchCord3(iirL1, 0, iirL2, 0);
AudioConnection_F32         patchCord10(iirL2, 0, multiplyL, 0);
AudioConnection_F32         patchCord4(carrier, 0, multiplyL, 1);
AudioConnection_F32         patchCord21(multiplyL, 0, mixerL, 1);  //end of ultrasound path
AudioConnection_F32         patchCord27(mixerL, 0, fastCompL, 0); //compression for whatever audio we're sending to the ears
AudioConnection_F32         patchCord5(fastCompL, 0, i2s_out, 0); //send to left output
//AudioConnection_F32         patchCord27(mixerL, 0, i2s_out, 0);  //send to left output

#if USE_STEREO
  //make all of the audio connections for the right audio channel
  AudioConnection_F32         patchCord2000(i2s_in, 1, mixerR, 0); //raw path for hear-thru
  AudioConnection_F32         patchCord101(i2s_in, 1, preGainR, 0); //processed path for ultrasound
  AudioConnection_F32         patchCord200(preGainR, 0, iirR1, 0);
  AudioConnection_F32         patchCord300(iirR1, 0, iirR2, 0);
  AudioConnection_F32         patchCord1000(iirR2, 0, multiplyR, 0);
  AudioConnection_F32         patchCord400(carrier, 0, multiplyR, 1);
  AudioConnection_F32         patchCord2100(multiplyR, 0, mixerR, 1);  //end of ultrasound path
  //AudioConnection_F32         patchCord2110(mixerR, 0, fastCompR, 0); //compression for whatever audio we're sending to the ears
  //AudioConnection_F32         patchCord500(fastCompR, 0, i2s_out, 1); //send to right output
  AudioConnection_F32         patchCord2110(mixerR,0,i2s_out,1);
#else
  //copy the left channel over to the right channel (ie, mono)
  //AudioConnection_F32         patchCord6(compWDRC_L, 0, i2s_out, 1);
  AudioConnection_F32         patchCord6(fastCompL, 0, i2s_out, 1);
#endif

//set the recording configuration
const int config_pcb_mics = 20;
const int config_mic_jack = 21;
const int config_line_in_SE  = 22;

int current_config = 0;
void setConfiguration(int config) { 
  switch (config) {
    case config_pcb_mics:
      //Select Input
      audioHardware.setInputGain_dB(0); delay(100); //make quieter
      audioHardware.inputSelect(TYMPAN_INPUT_ON_BOARD_MIC); // use the on-board microphones

      //Set input gain to 0dB
      input_gain_dB = 15;
      audioHardware.setInputGain_dB(input_gain_dB);

      //Store configuration
      current_config = config_pcb_mics;
      break;
      
    case config_mic_jack:

      //Select Input
      audioHardware.setInputGain_dB(0); delay(100); //make quieter
      audioHardware.inputSelect(TYMPAN_INPUT_JACK_AS_MIC); // use the mic jack
      audioHardware.setEnableStereoExtMicBias(true);

      //Set input gain to desired value
      input_gain_dB = default_input_gain_dB;
      audioHardware.setInputGain_dB(input_gain_dB);

      //Store configuration
      current_config = config_mic_jack;
      break;
      
    case config_line_in_SE:
      
      //Select Input
      audioHardware.setInputGain_dB(0); delay(100); //make quieter
      audioHardware.inputSelect(TYMPAN_INPUT_LINE_IN); // use the line-input through holes

      //Set input gain to desired value
      input_gain_dB = default_input_gain_dB;
      audioHardware.setInputGain_dB(input_gain_dB);

      //Store configuration
      current_config = config_line_in_SE;
      break;
  }
}

// define functions to setup the hardware
void setupAudioHardware(void) {

  //use Tympan Audio Board
  audioHardware.enable(); // activate AIC

  //choose input
  setConfiguration(config_pcb_mics);
  
  //set volumes
  audioHardware.volume_dB(0);  // -63.6 to +24 dB in 0.5dB steps.  uses signed 8-bit
  audioHardware.setInputGain_dB(input_gain_dB); // set MICPGA volume, 0-47.5dB in 0.5dB setps

}

//define my high-pass filter: [b,a]=butter(2,30000/(96000/2),'high')
float32_t hp_b[] = {0.186694333116378,  -0.373388666232757,   0.186694333116378};
float32_t hp_a[] = { 1.000000000000000,   0.462938025291041,   0.209715357756555};

float32_t carrier_freq_Hz = 37000.0f;

//define functions to setup the audio processing parameters
void setupAudioProcessing(void) {
  //set the pre-gain (if used)
  preGainL.setGain_dB(ultrasound_gain_dB);
  preGainR.setGain_dB(ultrasound_gain_dB);

  //setup the HP filter
  iirL1.setFilterCoeff_Matlab(hp_a, hp_b);
  iirL2.setFilterCoeff_Matlab(hp_a, hp_b); //appply the same filter a second time for steeper roll-off
  iirR1.setFilterCoeff_Matlab(hp_a, hp_b);
  iirR2.setFilterCoeff_Matlab(hp_a, hp_b); //appply the same filter a second time for steeper roll-off

  //setup the demodulation
  carrier.amplitude(1.0);  carrier.frequency(carrier_freq_Hz);

  //setup the fast compression
  float maxdB = 105.0;  //calibration factor.  What dB SPL is full scale?
  float exp_cr = 1.0, exp_end_knee = 0.0;    //expansion regime: compression ratio and knee point
  float tkgain = 0.0;     //compression-start gain (ie, gain of linear regime)
  float attack_ms = 1.0, release_ms = 100.0;
  float comp_ratio = 5.0; //compression regime: compression ratio
  float tk = 80.0;        //compression regime: compression knee point (dB SPL...related via maxdB)
  float bolt = 100.0;     //compression regime: output limiter
  fastCompL.setParams(attack_ms, release_ms, maxdB, exp_cr, exp_end_knee, tkgain, comp_ratio, tk, bolt);
  fastCompR.setParams(attack_ms, release_ms, maxdB, exp_cr, exp_end_knee, tkgain, comp_ratio, tk, bolt);   
  
  //setup mixer
  mixerL.gain(0, 0.0); //normal audio is channel 0
  mixerL.gain(1, 1.0); //demodulated ultrasound is channel 1
  mixerR.gain(0, 0.0); //normal audio is channel 0
  mixerR.gain(1, 1.0); //demodulated ultrasound is channel 1

}

//control display and serial interaction
bool enable_printCPUandMemory = false;
void togglePrintMemoryAndCPU(void) { enable_printCPUandMemory = !enable_printCPUandMemory;}; 
void setPrintMemoryAndCPU(bool state) { enable_printCPUandMemory = state; };
SerialManager serialManager(audioHardware);
#define BOTH_SERIAL audioHardware

// define the setup() function, the function that is called once when the device is booting
void setup() {
  //Setup serial communication
  audioHardware.beginBothSerial(); delay(2000);

  //print basic audio parameters
  BOTH_SERIAL.println(overall_name);
  BOTH_SERIAL.print("  Sample Rate (Hz): "); BOTH_SERIAL.println(audio_settings.sample_rate_Hz);
  BOTH_SERIAL.print("  Audio Block Size (samples): "); BOTH_SERIAL.println(audio_settings.audio_block_samples);
  #if USE_STEREO
    BOTH_SERIAL.println("  Running in Stereo.");
  #endif
  
  //allocate the audio memory
  AudioMemory_F32_wSettings(MAX_F32_BLOCKS, audio_settings); //I can only seem to allocate 400 blocks

  // Enable the audio shield, select input, and enable output
  setupAudioHardware();

  //setup filters and mixers
  setupAudioProcessing();
  
  BOTH_SERIAL.println("setup() complete");
} //end setup()


// define the loop() function, the function that is repeated over and over for the life of the device
void loop() {
  //choose to sleep ("wait for interrupt") instead of spinning our wheels doing nothing but consuming power
  asm(" WFI");  //ARM-specific.  Will wake on next interrupt.  The audio library issues tons of interrupts, so we wake up often.

  //respond to Serial commands
  while (Serial.available()) serialManager.respondToByte((char)Serial.read());   //USB Serial
  while (Serial1.available()) serialManager.respondToByte((char)Serial1.read()); //BT Serial
  
  //service the SD recording
  serviceSD();

  //update the memory and CPU usage...if enough time has passed
  if (enable_printCPUandMemory) printCPUandMemory(millis());

  //change the LED status
  serviceLEDs();

  //service the potentiometer...if enough time has passed
  servicePotentiometer(millis());
  

} //end loop()


//servicePotentiometer: listens to the blue potentiometer and sends the new pot value
//  to the audio processing algorithm as a control parameter
void servicePotentiometer(unsigned long curTime_millis) {
  static unsigned long updatePeriod_millis = 100; //how many milliseconds between updating the potentiometer reading?
  static unsigned long lastUpdate_millis = 0;
  static float prev_val = 0;

  //has enough time passed to update everything?
  if (curTime_millis < lastUpdate_millis) lastUpdate_millis = 0; //handle wrap-around of the clock
  if ((curTime_millis - lastUpdate_millis) > updatePeriod_millis) { //is it time to update the user interface?

    //read potentiometer
    float val = float(audioHardware.readPotentiometer()) / 1023.0; //0.0 to 1.0
    val = 0.1 * (float)((int)(10.0 * val + 0.5)); //quantize so that it doesn't chatter...0 to 1.0

    //send the potentiometer value to your algorithm as a control parameter
    //float scaled_val = val / 3.0; scaled_val = scaled_val * scaled_val;
    if (abs(val - prev_val) > 0.05) { //is it different than befor?
      prev_val = val;  //save the value for comparison for the next time around


      #if 0
        //change the volume
        float vol_dB = 0.f + 30.0f * ((val - 0.5) * 2.0); //set volume as 0dB +/- 30 dB
        BOTH_SERIAL.print("Changing output volume frequency to = "); BOTH_SERIAL.print(vol_dB); BOTH_SERIAL.println(" dB");
        audioHardware.volume_dB(vol_dB);
      #else
        //change the carrier
        float freq = 30000 + 10000.f * val; //change tone carrier_Hz 30000-40000
        BOTH_SERIAL.print("Changing carrier frequency to = "); BOTH_SERIAL.println(freq);
        carrier.frequency(freq);
        if (val < 0.025) {
          mixerL.gain(0, 1.0);  mixerL.gain(1, 0.0); //switch to normal audio
          mixerR.gain(0, 1.0);  mixerR.gain(1, 0.0); //switch to normal audio
        } else {
          mixerL.gain(0, 0.0);  mixerL.gain(1, 1.0); //switch to demodulated ultrasound
          mixerR.gain(0, 0.0);  mixerR.gain(1, 1.0); //switch to demodulated ultrasound
        }
      #endif
      
    }
    lastUpdate_millis = curTime_millis;
  } // end if
} //end servicePotentiometer();


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
    BOTH_SERIAL.print("CPU Cur/Peak: ");
    BOTH_SERIAL.print(audio_settings.processorUsage());
    BOTH_SERIAL.print("%/");
    BOTH_SERIAL.print(audio_settings.processorUsageMax());
    BOTH_SERIAL.print("%, ");
    BOTH_SERIAL.print("Dyn MEM Float32 Cur/Peak: ");
    BOTH_SERIAL.print(AudioMemoryUsage_F32());
    BOTH_SERIAL.print("/");
    BOTH_SERIAL.print(AudioMemoryUsageMax_F32());
    BOTH_SERIAL.println();
}

void serviceLEDs(void) {
  if (current_SD_state == STATE_UNPREPARED) {
    audioHardware.setRedLED(HIGH); audioHardware.setAmberLED(HIGH); //Turn ON both
  } else if (current_SD_state == STATE_RECORDING) {
    audioHardware.setRedLED(LOW); audioHardware.setAmberLED(HIGH); //Go Amber
  } else {
    audioHardware.setRedLED(HIGH); audioHardware.setAmberLED(LOW); //Go Red
  }
}

//here's a function to change the volume settings.   We'll also invoke it from our serialManager
float incrementInputGain(float increment_dB) {
  input_gain_dB += increment_dB;
  if (input_gain_dB < 0.0) {
    BOTH_SERIAL.println("Error: cannot set input gain less than 0 dB.");
    BOTH_SERIAL.println("Setting input gain to 0 dB.");
    input_gain_dB = 0.0;
  }
  audioHardware.setInputGain_dB(input_gain_dB);
  return input_gain_dB;
}
float incrementUltrasoundGain(float increment_dB) {
  ultrasound_gain_dB += increment_dB;
  preGainL.setGain_dB(ultrasound_gain_dB);
  preGainR.setGain_dB(ultrasound_gain_dB);
  return ultrasound_gain_dB;
}
// //////////////////////////////////// Control the audio processing from the SerialManager
void setOutputMute(void) {
  mixerL.gain(0, 0.0);  mixerL.gain(1, 0.0); //mute all
  mixerR.gain(0, 0.0);  mixerR.gain(1, 0.0); //mute all
}
void setOutputAudio(bool state) {
  if (state) {
    mixerL.gain(0, 1.0);  mixerR.gain(0, 1.0);  //turn on 
  } else {
    mixerL.gain(0, 0.0);  mixerR.gain(0, 0.0);  //turn off 
  }
}
void setOutputUltrasound(bool state) {
  if (state) {
    mixerL.gain(1, 1.0);  mixerR.gain(1, 1.0);  //turn on 
  } else {
    mixerL.gain(1, 0.0);  mixerR.gain(1, 0.0);  //turn off 
  }
}

