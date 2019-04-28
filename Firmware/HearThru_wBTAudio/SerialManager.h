
#ifndef _SerialManager_h
#define _SerialManager_h

#include <Tympan_Library.h>


//define state
#define NO_STATE (-1)
class State_t {
  public:
    int input_source = NO_STATE;
    int audio = NO_STATE;
    int alg = NO_STATE;
};

//Extern variables
extern Tympan myTympan;
extern SDAudioWriter_F32asI16 stereoSDWriter;
extern float vol_knob_gain_dB;
extern float input_gain_dB;
extern State_t myState;
extern const int INPUT_PCBMICS;
extern const int INPUT_MICJACK;
extern const int INPUT_LINEIN_SE;
extern const int AUDIO_MUTE;
extern const int AUDIO_MONO;
extern const int AUDIO_STEREO;
extern const int ALG_LINEAR;
extern const int ALG_FASTCOMP;
extern const int ALG_SLOWCOMP;

//Extern Functions
extern void setConfiguration(int);
extern void togglePrintMemoryAndCPU(void);
extern void setPrintMemoryAndCPU(bool);
//extern void togglePrintAveSignalLevels(bool);
//extern void beginRecordingProcess(void);
//extern void stopRecording(void);
extern void incrementInputGain(float);
//extern void prepareSDforRecording(void);
extern void setAudioMute(void);
extern void setAudioMono(void);
extern void setAudioStereo(void);
extern void setAudioLinear(void);
extern void setAudioFastComp(void);
extern void setAudioSlowComp(void);
extern void scaleCompressionSpeed(float,bool);
extern void incrementKneepoint(float,bool);

//now, define the Serial Manager class
class SerialManager {
  public:
    SerialManager(void) {  };

    void respondToByte(char c);
    void printHelp(void);
    void printFullGUIState(void);
    void printGainSettings(void);
    void setButtonState(String btnId, bool newState);
    float gainIncrement_dB = 2.5f;
    float compScaleFactor = 0.5; //twice as fast
    float kneeIncrement_dB = 5.0f;

  private:

};

void SerialManager::printHelp(void) {
  myTympan.println();
  myTympan.println("SerialManager Help: Available Commands:");
  //myTympan.println("   J: Print the JSON config object, for the Tympan Remote app");
  //myTympan.println("    j: Print the button state for the Tympan Remote app");
  myTympan.println("   C: Toggle printing of CPU and Memory usage");
  myTympan.println("   w: Switch Input to PCB Mics");
  myTympan.println("   W: Switch Input to Headset Mics");
  myTympan.print  ("   i: Input: Increase gain by "); myTympan.print(gainIncrement_dB); myTympan.println(" dB");
  myTympan.print  ("   I: Input: Decrease gain by "); myTympan.print(gainIncrement_dB); myTympan.println(" dB");
  myTympan.println("   q: Input: Mute");
  myTympan.println("   m: Input: Mono");
  myTympan.println("   M: Input: Stereo");
  myTympan.println("   l: Processing: linear.");
  myTympan.println("   k: Processing: Fast-compression.");
  myTympan.println("   K: Processing: Slow-compression.");
  myTympan.println("   a: Compression: make 2x faster.");
  myTympan.println("   A: Compression: make 2x slower.");
  myTympan.println("   b: Compression: increase kneepoint.");
  myTympan.println("   B: Compression: decrease kneebpoint.");
  myTympan.println("   p: SD: prepare for recording");
  myTympan.println("   r: SD: begin recording");
  myTympan.println("   s: SD: stop recording");
  myTympan.println("   h: Print this help");


  myTympan.println();
}


//switch yard to determine the desired action
void SerialManager::respondToByte(char c) {
  switch (c) {
    case 'h': case '?':
      printHelp(); break;
    case 'c':
      myTympan.println("Received: start CPU reporting");
      setPrintMemoryAndCPU(true);
      setButtonState("cpuStart",true);
      break;
    case 'C':
      myTympan.println("Received: stop CPU reporting");
      setPrintMemoryAndCPU(false);
      setButtonState("cpuStart",false);
      break;
    case 'i':
      incrementInputGain(gainIncrement_dB);
      printGainSettings();
      break;
    case 'I':   //which is "shift i"
      incrementInputGain(-gainIncrement_dB);
      printGainSettings();
      break;
    case 'a':
      //increase compression speed (make it faster, so compSaleFactor should be less than 1.0)
      scaleCompressionSpeed(compScaleFactor,true); //the "true" is to print out the new values
      break;
     case 'A':
      //decrease compression speed (make it slower, so compSaleFactor should be greater than 1.0)
      scaleCompressionSpeed(1.0/compScaleFactor,true); //the "true" is to print out the new values
      break; 
    case 'b':
      //increase
      incrementKneepoint(kneeIncrement_dB,true);   //the "true" is to print out the new values
      break;
    case 'B':
      //decrease
      incrementKneepoint(-kneeIncrement_dB,true);   //the "true" is to print out the new values
      break;    
    case 'q':
      myTympan.println("Received: Muting");
      setAudioMute();
      setButtonState("mute",true);delay(20);
      setButtonState("mono",false);delay(20);
      setButtonState("stereo",false);
      break;
    case 'm':
      myTympan.println("Received: Mono");
      setAudioMono();
      setButtonState("mute",false);delay(20);
      setButtonState("mono",true);delay(20);
      setButtonState("stereo",false);
      break;      
    case 'M':
      myTympan.println("Received: Stereo");
      setAudioStereo();
      setButtonState("mute",false);delay(20);
      setButtonState("mono",false);delay(20);
      setButtonState("stereo",true);
      break;
    case  'l':
      myTympan.println("Received: Linear processing");
      setAudioLinear();
      setButtonState("linear",true);delay(20);
      setButtonState("fast",false);delay(20);
      setButtonState("slow",false);
      break;
    case 'k':
      myTympan.println("Received: Fast compression");
      setAudioFastComp();
      setButtonState("linear",false);delay(20);
      setButtonState("fast",true);delay(20);
      setButtonState("slow",false);
      break;
    case 'K':
      myTympan.println("Received: Slow compression");
      setAudioSlowComp();
      setButtonState("linear",false);delay(20);
      setButtonState("fast",false);delay(20);
      setButtonState("slow",true);
      break;
    case 'w':
      myTympan.println("Received: PCB Mics");
      setConfiguration(INPUT_PCBMICS);
      setButtonState("configPCB",true);
      setButtonState("configHeadset",false);
      break;
    case 'W':
      myTympan.println("Recevied: Headset Mics.");
      setConfiguration(INPUT_MICJACK);
      setButtonState("configPCB",false);
      setButtonState("configHeadset",true);
      break;
    case 'p':
      myTympan.println("Received: prepare SD for recording");
      //prepareSDforRecording();
      stereoSDWriter.prepareSDforRecording();
      break;
    case 'r':
      myTympan.println("Received: begin SD recording");
      //beginRecordingProcess();
      stereoSDWriter.startRecording();
      setButtonState("recordStart",true);
      break;
    case 's':
      myTympan.println("Received: stop SD recording");
      stereoSDWriter.stopRecording();
      setButtonState("recordStart",false);
      break;
    case 'J':
      {
        // Print the layout for the Tympan Remote app, in a JSON-ish string
        // (single quotes are used here, whereas JSON spec requires double quotes.  The app converts ' to " before parsing the JSON string).
        // If you don't give a button an id, then it will be assigned the id 'default'.  IDs don't have to be unique; if two buttons have the same id,
        // then you always control them together and they'll always have the same state. 
        // Please don't put commas or colons in your ID strings!
        // The 'icon' is how the device appears in the app - could be an icon, could be a pic of the device.  Put the
        // image in the TympanRemote app in /src/assets/devIcon/ and set 'icon' to the filename.
        char jsonConfig[] = "JSON={"
          "'icon':'tympan.png',"
          "'pages':["
            "{'title':'Presets','cards':["
              "{'name':'Audio Type','buttons':[{'label': 'Mute', 'cmd': 'q', 'id': 'mute'},{'label': 'Mono', 'cmd': 'm', 'id': 'mono'},{'label': 'Stereo', 'cmd': 'M', 'id': 'stereo'}]},"
              "{'name':'Audio Processing','buttons':[{'label': 'Linear','cmd': 'l', 'id': 'linear'},{'label': 'Fast-Comp','cmd': 'k', 'id': 'fast'},{'label': 'Slow-Comp','cmd': 'K', 'id': 'slow'}]}"
            "]},"
            "{'title':'Tuner','cards':["
              "{'name':'Select Input','buttons':[{'label': 'Headset Mics', 'cmd': 'W', 'id':'configHeadset'},{'label': 'PCB Mics', 'cmd': 'w', 'id': 'configPCB'}]},"
              "{'name':'Input Gain', 'buttons':[{'label': 'Less', 'cmd' :'I'},{'label': 'More', 'cmd': 'i'}]},"
              "{'name':'Record Mics to SD Card','buttons':[{'label': 'Prepare', 'cmd': 'p'},{'label': 'Start', 'cmd': 'r', 'id':'recordStart'},{'label': 'Stop', 'cmd': 's'}]},"
              "{'name':'CPU Reporting', 'buttons':[{'label': 'Start', 'cmd' :'c','id':'cpuStart'},{'label': 'Stop', 'cmd': 'C'}]},"
              "{'name':'Compression: Attack and Release', 'buttons':[{'label': 'Slower','cmd':'A'},{'label':'Faster','cmd':'a'}]},"
              "{'name':'Compression: Kneepoint','buttons':[{'label': 'Lower','cmd':'B'},{'label':'Higher','cmd':'b'}]}"
            "]}"                            
          "]"
        "}";
        myTympan.println(jsonConfig);
        delay(100);
        printFullGUIState();
        break;
      }
  }
}

void SerialManager::printFullGUIState(void) {
  switch (myState.audio) {
    case (AUDIO_MUTE):
      setButtonState("mute",true);delay(20);
      setButtonState("mono",false);delay(20);
      setButtonState("stereo",false);
      break;
    case (AUDIO_MONO):
      setButtonState("mute",false);delay(20);
      setButtonState("mono",true);delay(20);
      setButtonState("stereo",false);
      break;
    case (AUDIO_STEREO):
      setButtonState("mute",false);delay(20);
      setButtonState("mono",false);delay(20);
      setButtonState("stereo",true);
      break;
  }
  switch (myState.alg) {
    case (ALG_LINEAR):
      setButtonState("linear",true);delay(20);
      setButtonState("fast",false);delay(20);
      setButtonState("slow",false);
      break;
    case (ALG_FASTCOMP):
      setButtonState("linear",false);delay(20);
      setButtonState("fast",true);delay(20);
      setButtonState("slow",false);
      break;
    case (ALG_SLOWCOMP):
      setButtonState("linear",false);delay(20);
      setButtonState("fast",false);delay(20);
      setButtonState("slow",true);
      break;
  }
  switch (myState.input_source) {
    case (INPUT_PCBMICS):
      setButtonState("configPCB",true); delay(20);
      setButtonState("configHeadset",false);
      break;
    case (INPUT_MICJACK): 
      setButtonState("configPCB",false); delay(20);
      setButtonState("configHeadset",true);
      break;
  }
}

void SerialManager::printGainSettings(void) {
  myTympan.print("Vol Knob = ");
  myTympan.print(vol_knob_gain_dB, 1);
  myTympan.print(", Input PGA = ");
  myTympan.print(input_gain_dB, 1);
  myTympan.println();
}

void SerialManager::setButtonState(String btnId, bool newState) {
  if (newState) {
    myTympan.println("STATE=BTN:" + btnId + ":1");
  } else {
    myTympan.println("STATE=BTN:" + btnId + ":0");
  }
}

#endif
