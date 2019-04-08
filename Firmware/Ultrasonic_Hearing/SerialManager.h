
#ifndef _SerialManager_h
#define _SerialManager_h

#include <Tympan_Library.h>

//add in the algorithm whose gains we wish to set via this SerialManager...change this if your gain algorithms class changes names!
//include "AudioEffectCompWDRC_F32.h"    //change this if you change the name of the algorithm's source code filename
//typedef AudioEffectCompWDRC_F32 GainAlgorithm_t; //change this if you change the algorithm's class name

//now, define the Serial Manager class
class SerialManager {
  public:
    SerialManager(TympanBase &_audioHardware)
      : audioHardware(_audioHardware)
    {  };

    void respondToByte(char c);
    void printHelp(void);
    void printGainSettings(void);
    void setButtonState(String btnId, bool newState);
    float gainIncrement_dB = 2.5f;

  private:
    TympanBase &audioHardware;

};

void SerialManager::printHelp(void) {
  audioHardware.println();
  audioHardware.println("SerialManager Help: Available Commands:");
  audioHardware.println("   J: Print the JSON config object, for the Tympan Remote app");
  audioHardware.println("   C: Toggle printing of CPU and Memory usage");
  audioHardware.println("   p: SD: prepare for recording");
  audioHardware.println("   r: SD: begin recording");
  audioHardware.println("   s: SD: stop recording");
  audioHardware.print  ("   i: Input: Increase gain by "); audioHardware.print(gainIncrement_dB); audioHardware.println(" dB");
  audioHardware.print  ("   I: Input: Decrease gain by "); audioHardware.print(gainIncrement_dB); audioHardware.println(" dB");
  //audioHardware.println("   q: Input: Mute");
  //audioHardware.println("   m: Input: Mono");
  //audioHardware.println("   M: Input: Stereo");
  //audioHardware.println("   l: Processing: linear.");
  //audioHardware.println("   k: Processing: Fast-compression.");
  //audioHardware.println("   K: Processing: Slow-compression.");
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
extern float incrementInputGain(float);
extern float incrementUltrasoundGain(float);
extern void prepareSDforRecording(void);
extern void setOutputMute(void);
extern void setOutputAudio(bool);
extern void setOutputUltrasound(bool);
//extern void setAudioLinear(void);
//extern void setAudioFastComp(void);
//extern void setAudioSlowComp(void);

//Extern variables
extern float vol_knob_gain_dB;
extern float input_gain_dB;
extern const int config_pcb_mics;
extern const int config_mic_jack;
extern const int config_line_in_SE;
//extern const int config_line_in_diff;


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
    case 'o':
      audioHardware.print("Ultrasound Gain (dB) = "); 
      audioHardware.println(incrementUltrasoundGain(gainIncrement_dB));
      break;
    case 'O':
      audioHardware.print("Ultrasound Gain (dB) = "); 
      audioHardware.println(incrementUltrasoundGain(-gainIncrement_dB));
      break;
    case 'w':
      audioHardware.println("Received: PCB Mics");
      setConfiguration(config_pcb_mics);
      setButtonState("configPCB",true);
      setButtonState("configHeadset",false);
      break;
    case 'W':
      audioHardware.println("Recevied: Headset Mics.");
      setConfiguration(config_mic_jack);
      setButtonState("configPCB",false);
      setButtonState("configHeadset",true);
      break;
    case 'q':
      audioHardware.println("Received: Mute Audio");
      setOutputAudio(false);
      setButtonState("muteAudio",true);
      setButtonState("enableAudio",false);
      break;   
    case 'Q':
      audioHardware.println("Received: Enable Audio");
      setOutputAudio(true);
      setButtonState("muteAudio",false);
      setButtonState("enableAudio",true);
      break;
    case 'm':
      audioHardware.println("Received: Mute Ultrasound");
      setOutputUltrasound(false);
      setButtonState("muteUltra",true);
      setButtonState("enableUltra",false);
      break;   
    case 'M':
      audioHardware.println("Received: Enable Ultrasound");
      setOutputUltrasound(true);
      setButtonState("muteUltra",false);
      setButtonState("enableUltra",true);
      break;
//    case  'l':
//      audioHardware.println("Received: Linear processing");
//      setAudioLinear();
//      setButtonState("fast",false);
//      setButtonState("slow",false);
//      setButtonState("linear",true);
//      break;
//    case 'k':
//      audioHardware.println("Received: Fast compression");
//      setAudioFastComp();
//      setButtonState("fast",true);
//      setButtonState("slow",false);
//      setButtonState("linear",false);
//      break;
//    case 'K':
//      audioHardware.println("Received: Slow compression");
//      setAudioSlowComp();
//      setButtonState("fast",false);
//      setButtonState("slow",true);
//      setButtonState("linear",false);
//      break;
    case 'p':
      audioHardware.println("Received: prepare SD for recording");
      prepareSDforRecording();
      setButtonState("recordStop",true);
      break;
    case 'r':
      audioHardware.println("Received: begin SD recording");
      beginRecordingProcess();
      setButtonState("recordStart",true);
      setButtonState("recordStop",false);
      break;
    case 's':
      audioHardware.println("Received: stop SD recording");
      stopRecording();
      setButtonState("recordStart",false);
      setButtonState("recordStop",true);
      break;
    case 'J':
      {
        // Print the layout for the Tympan Remote app, in a JSON-ish string
        // (single quotes are used here, whereas JSON spec requires double quotes.  The app converts ' to " before parsing the JSON string).
        // If you don't give a button an id, then it will be assigned the id 'default'.  IDs don't have to be unique; if two buttons have the same id,
        // then you always control them together and they'll always have the same state. 
        // Please don't put commas or colons in your ID strings!
        #if 0
        char jsonConfig[] = "JSON={'pages':["
                              "{'title':'Presets','cards':["
                                "{'name':'Audio Type','buttons':[{'label': 'Mute', 'cmd': 'q', 'id': 'mute'},{'label': 'Mono', 'cmd': 'm', 'id': 'mono'},{'label': 'Stereo', 'cmd': 'M', 'id': 'stereo'}]},"
                                "{'name':'Audio Processing','buttons':[{'label': 'Linear','cmd': 'l', 'id': 'linear'},{'label': 'Fast-Comp','cmd': 'k', 'id': 'fast'},{'label': 'Slow-Comp','cmd': 'K', 'id': 'slow'}]}"
                              "]},"
                              "{'title':'Tuner','cards':["
                                "{'name':'Record Mics to SD Card','buttons':[{'label': 'Prepare', 'cmd': 'p'},{'label': 'Start', 'cmd': 'r', 'id':'recordStart'},{'label': 'Stop', 'cmd': 's', 'id':'recordStop'}]},"
                                "{'name':'Microphone Gain', 'buttons':[{'label': 'Less', 'cmd' :'I'},{'label': 'More', 'cmd': 'i'}]},"
                                "{'name':'CPU Reporting', 'buttons':[{'label': 'Start', 'cmd' :'c','id':'cpuStart'},{'label': 'Stop', 'cmd': 'C'}]}"
                              "]}"                            
                            "]}";
        #else
        char jsonConfig[] = "JSON={'pages':["
                              "{'title':'Presets','cards':["
                                "{'name':'Hear-Thru','buttons':[{'label': 'Mute', 'cmd': 'q', 'id': 'muteAudio'},{'label': 'Audio', 'cmd': 'Q', 'id': 'enableAudio'}]},"
                                "{'name':'Ultrasound','buttons':[{'label': 'Mute', 'cmd': 'm', 'id': 'muteUltra'},{'label': 'Ultrasound', 'cmd': 'M', 'id': 'enableUltra'}]}"
                              "]},"
                              "{'title':'Tuner','cards':["
                                "{'name':'Select Input','buttons':[{'label': 'PCB Mics', 'cmd': 'w', 'id': 'configPCB'},{'label': 'Headset Mics', 'cmd': 'W', 'id':'configHeadset'}]},"
                                "{'name':'Record Mics to SD Card','buttons':[{'label': 'Prepare', 'cmd': 'p'},{'label': 'Start', 'cmd': 'r', 'id':'recordStart'},{'label': 'Stop', 'cmd': 's', 'id':'recordStop'}]},"
                                "{'name':'Microphone Input Gain', 'buttons':[{'label': 'Less', 'cmd' :'I'},{'label': 'More', 'cmd': 'i'}]},"
                                "{'name':'Extra Ultrasound Gain', 'buttons':[{'label': 'Less', 'cmd' :'O'},{'label': 'More', 'cmd': 'o'}]},"
                                "{'name':'CPU Reporting', 'buttons':[{'label': 'Start', 'cmd' :'c','id':'cpuStart'},{'label': 'Stop', 'cmd': 'C'}]}"
                              "]}"                            
                            "]}";
        #endif
        audioHardware.println(jsonConfig);
        break;
      }
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
