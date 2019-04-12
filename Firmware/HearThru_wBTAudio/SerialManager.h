
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

//now, define the Serial Manager class
class SerialManager {
  public:
    SerialManager(TympanBase &_audioHardware)
      : audioHardware(_audioHardware)
    {  };

    void respondToByte(char c);
    void printHelp(void);
    void printFullGUIState(void);
    void printGainSettings(void);
    void setButtonState(String btnId, bool newState);
    float gainIncrement_dB = 2.5f;

  private:
    TympanBase &audioHardware;

};

void SerialManager::printHelp(void) {
  audioHardware.println();
  audioHardware.println("SerialManager Help: Available Commands:");
  //audioHardware.println("   J: Print the JSON config object, for the Tympan Remote app");
  //audioHardware.println("    j: Print the button state for the Tympan Remote app");
  audioHardware.println("   C: Toggle printing of CPU and Memory usage");
  audioHardware.println("   w: Switch Input to PCB Mics");
  audioHardware.println("   W: Switch Input to Headset Mics");
  audioHardware.print  ("   i: Input: Increase gain by "); audioHardware.print(gainIncrement_dB); audioHardware.println(" dB");
  audioHardware.print  ("   I: Input: Decrease gain by "); audioHardware.print(gainIncrement_dB); audioHardware.println(" dB");
  audioHardware.println("   q: Input: Mute");
  audioHardware.println("   m: Input: Mono");
  audioHardware.println("   M: Input: Stereo");
  audioHardware.println("   l: Processing: linear.");
  audioHardware.println("   k: Processing: Fast-compression.");
  audioHardware.println("   K: Processing: Slow-compression.");
  audioHardware.println("   p: SD: prepare for recording");
  audioHardware.println("   r: SD: begin recording");
  audioHardware.println("   s: SD: stop recording");
  audioHardware.println("   h: Print this help");


  audioHardware.println();
}



//Extern Functions
extern void setConfiguration(int config);
extern void togglePrintMemoryAndCPU(void);
extern void setPrintMemoryAndCPU(bool);
//extern void togglePrintAveSignalLevels(bool);
extern void beginRecordingProcess(void);
extern void stopRecording(void);
extern void incrementInputGain(float);
extern void prepareSDforRecording(void);
extern void setAudioMute(void);
extern void setAudioMono(void);
extern void setAudioStereo(void);
extern void setAudioLinear(void);
extern void setAudioFastComp(void);
extern void setAudioSlowComp(void);

//Extern variables
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

//switch yard to determine the desired action
void SerialManager::respondToByte(char c) {
  switch (c) {
    case 'h': case '?':
      printHelp(); break;
    case 'c':
      audioHardware.println("Received: start CPU reporting");
      setPrintMemoryAndCPU(true);
      setButtonState("cpuStart",true);
      break;
    case 'C':
      audioHardware.println("Received: stop CPU reporting");
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
    case 'q':
      audioHardware.println("Received: Muting");
      setAudioMute();
      setButtonState("mute",true);delay(20);
      setButtonState("mono",false);delay(20);
      setButtonState("stereo",false);
      break;
    case 'm':
      audioHardware.println("Received: Mono");
      setAudioMono();
      setButtonState("mute",false);delay(20);
      setButtonState("mono",true);delay(20);
      setButtonState("stereo",false);
      break;      
    case 'M':
      audioHardware.println("Received: Stereo");
      setAudioStereo();
      setButtonState("mute",false);delay(20);
      setButtonState("mono",false);delay(20);
      setButtonState("stereo",true);
      break;
    case  'l':
      audioHardware.println("Received: Linear processing");
      setAudioLinear();
      setButtonState("linear",true);delay(20);
      setButtonState("fast",false);delay(20);
      setButtonState("slow",false);
      break;
    case 'k':
      audioHardware.println("Received: Fast compression");
      setAudioFastComp();
      setButtonState("linear",false);delay(20);
      setButtonState("fast",true);delay(20);
      setButtonState("slow",false);
      break;
    case 'K':
      audioHardware.println("Received: Slow compression");
      setAudioSlowComp();
      setButtonState("linear",false);delay(20);
      setButtonState("fast",false);delay(20);
      setButtonState("slow",true);
      break;
    case 'w':
      audioHardware.println("Received: PCB Mics");
      setConfiguration(INPUT_PCBMICS);
      setButtonState("configPCB",true);
      setButtonState("configHeadset",false);
      break;
    case 'W':
      audioHardware.println("Recevied: Headset Mics.");
      setConfiguration(INPUT_MICJACK);
      setButtonState("configPCB",false);
      setButtonState("configHeadset",true);
      break;
    case 'p':
      audioHardware.println("Received: prepare SD for recording");
      prepareSDforRecording();
      break;
    case 'r':
      audioHardware.println("Received: begin SD recording");
      beginRecordingProcess();
      setButtonState("recordStart",true);
      break;
    case 's':
      audioHardware.println("Received: stop SD recording");
      stopRecording();
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
              "{'name':'CPU Reporting', 'buttons':[{'label': 'Start', 'cmd' :'c','id':'cpuStart'},{'label': 'Stop', 'cmd': 'C'}]}"
            "]}"                            
          "]"
        "}";
        audioHardware.println(jsonConfig);
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
  audioHardware.print("Vol Knob = ");
  audioHardware.print(vol_knob_gain_dB, 1);
  audioHardware.print(", Input PGA = ");
  audioHardware.print(input_gain_dB, 1);
  audioHardware.println();
}

void SerialManager::setButtonState(String btnId, bool newState) {
  if (newState) {
    audioHardware.println("STATE=BTN:" + btnId + ":1");
  } else {
    audioHardware.println("STATE=BTN:" + btnId + ":0");
  }
}

#endif
