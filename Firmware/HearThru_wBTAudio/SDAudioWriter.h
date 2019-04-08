
/*
   Chip Audette, OpenAudio, Mar 2018

   MIT License.  Use at your own risk.
*/


//variables needed from the main file
extern AudioSettings_F32 audio_settings;
extern AudioInputI2S_F32 i2s_in;
extern AudioRecordQueue_F32 queueL, queueR;
extern TympanBase audioHardware;
#define BOTH_SERIAL audioHardware

//variables to control printing of warnings and timings and whatnot
#define PRINT_OVERRUN_WARNING 1   //set to 1 to print a warning that the there's been a hiccup in the writing to the SD.
#define PRINT_FULL_SD_TIMING 0    //set to 1 to print timing information of *every* write operation.  Great for logging to file.  Bad for real-time human reading.

////////////////////////////////////////////////////////////

#ifndef _SDAudioWriter_h
#define _SDAudioWriter_h

//include <SdFat_Gre.h>       //originally from https://github.com/greiman/SdFat  but class names have been modified to prevent collisions with Teensy Audio/SD libraries
#include <Tympan_Library.h>  //for data types float32_t and int16_t and whatnot
#include <AudioStream.h>     //for AUDIO_BLOCK_SAMPLES

// Preallocate 40MB file.
const uint64_t PRE_ALLOCATE_SIZE = 40ULL << 20;
#define MAXFILE 100

//char *header = 0;
#define MAX_AUDIO_BUFF_LEN (2*AUDIO_BLOCK_SAMPLES)  //Assume stereo (for the 2x).  AUDIO_BLOCK_SAMPLES is from Audio_Stream.h, which should be max of Audio_Stream_F32 as well.

class SDAudioWriter
{
  public:
    SDAudioWriter() {};
    ~SDAudioWriter() {
      if (isFileOpen()) {
        close();
      }
    }
    void setup(void) {
      init();
    };
    void init() {
      if (!sd.begin()) sd.errorHalt("sd.begin failed");
    }
    //write two F32 channels as int16
    int writeF32AsInt16(float32_t *chan1, float32_t *chan2, int nsamps) {
      const int buffer_len = 2 * nsamps; //it'll be stereo, so 2*nsamps
      int count = 0;
      if (file.isOpen()) {
        for (int Isamp = 0; Isamp < nsamps; Isamp++) {
          //convert the F32 to Int16 and interleave
          write_buffer[count++] = (int16_t)(chan1[Isamp] * 32767.0);
          write_buffer[count++] = (int16_t)(chan2[Isamp] * 32767.0);
        }

        // write all audio bytes (512 bytes is most efficient)
        if (flagPrintElapsedWriteTime) { usec = 0; }
        file.write((byte *)write_buffer, buffer_len * sizeof(write_buffer[0]));
        nBlocksWritten++;
        if (flagPrintElapsedWriteTime) { Serial.print("SD, us="); Serial.println(usec);  }
      }
      return 0;
    }

    bool open(char *fname) {
      if (sd.exists(fname)) {  //maybe this isn't necessary when using the O_TRUNC flag below
        // The SD library writes new data to the end of the
        // file, so to start a new recording, the old file
        // must be deleted before new data is written.
        sd.remove(fname);
      }
      
      #if 1
        file.open(fname,O_RDWR | O_CREAT | O_TRUNC);
      #else
        file.createContiguous(fname,PRE_ALLOCATE_SIZE);
      #endif 
 
      return isFileOpen();
    }

    int close(void) {
      //file.truncate();
      file.close();
      return 0;
    }
    bool isFileOpen(void) {
      if (file.isOpen()) {
        return true;
      } else {
        return false;
      }
    }
//    char* makeFilename(char * filename)
//    { static int ifl = 0;
//      ifl++;
//      if (ifl > MAXFILE) return 0;
//      sprintf(filename, "File%04d.raw", ifl);
//      //Serial.println(filename);
//      return filename;
//    }

    void enablePrintElapsedWriteTime(void) {
      flagPrintElapsedWriteTime = true;
    }
    void disablePrintElapseWriteTime(void) {
      flagPrintElapsedWriteTime = false;
    }
    unsigned long getNBlocksWritten(void) {
      return nBlocksWritten;
    }
    void resetNBlocksWritten(void) {
      nBlocksWritten = 0;
    }

  private:
    //SdFatSdio sd; //slower
    SdFatSdioEX sd; //faster
    SdFile_Gre file;
    int16_t write_buffer[MAX_AUDIO_BUFF_LEN];
    boolean flagPrintElapsedWriteTime = false;
    elapsedMicros usec;
    unsigned long nBlocksWritten = 0;

};

#endif

// /////////////////////////////////////////////////////////////


// Create variables to decide how long to record to SD
SDAudioWriter my_SD_writer;

// control the recording phases
#define STATE_UNPREPARED -1
#define STATE_STOPPED 0
#define STATE_BEGIN 1
#define STATE_RECORDING 2
#define STATE_CLOSE 3
int current_SD_state = STATE_UNPREPARED;
uint32_t recording_start_time_msec = 0;

void prepareSDforRecording(void) {
  if (current_SD_state == STATE_UNPREPARED) {
    my_SD_writer.init();
    if (PRINT_FULL_SD_TIMING) my_SD_writer.enablePrintElapsedWriteTime(); //for debugging.  make sure time is less than (audio_block_samples/sample_rate_Hz * 1e6) = 2900 usec for 128 samples at 44.1 kHz
    current_SD_state = STATE_STOPPED;
  }
}

int recording_count = 0;
void startRecording(void) {
  if (current_SD_state == STATE_BEGIN) {
    recording_count++; 
    char fname[] = "RECORDxx.RAW";
    int tens = recording_count / 10;  //truncates
    fname[6] = tens + '0';  //stupid way to convert the number to a character
    int ones = recording_count - tens*10;
    fname[7] = ones + '0';  //stupid way to convert the number to a character
    
    if (my_SD_writer.open(fname)) {
      BOTH_SERIAL.print("startRecording: Opened "); BOTH_SERIAL.print(fname); BOTH_SERIAL.println(" on SD for writing.");
      queueL.begin(); queueR.begin();
      //audioHardware.setRedLED(LOW); audioHardware.setAmberLED(HIGH); //Turn ON the Amber LED
      recording_start_time_msec = millis();
      current_SD_state = STATE_RECORDING;
    } else {
      BOTH_SERIAL.print("startRecording: Failed to open "); BOTH_SERIAL.print(fname); BOTH_SERIAL.println(" on SD for writing.");
    }

  } else {
    BOTH_SERIAL.println("startRecording: not in correct state to start recording.");
  }
}


void beginRecordingProcess(void) {
  if (current_SD_state == STATE_STOPPED) {
    current_SD_state = STATE_BEGIN;  
    startRecording();
  } else {
    BOTH_SERIAL.println("beginRecordingProcess: unprepared, already recording, or completed.");
  }
}


void stopRecording(void) {
  if (current_SD_state == STATE_RECORDING) {
      BOTH_SERIAL.println("stopRecording: Closing SD File...");
      my_SD_writer.close();
      queueL.end();  queueR.end();
      //audioHardware.setRedLED(HIGH); audioHardware.setAmberLED(LOW); //Turn OFF the Amber LED
      current_SD_state = STATE_STOPPED;
  }
}



void serviceSD(void) {
  if (my_SD_writer.isFileOpen()) {
    //if audio data is ready, write it to SD
    if ((queueL.available()) && (queueR.available())) {
      //my_SD_writer.writeF32AsInt16(queueLeft.readBuffer(),audio_block_samples);  //mono
      my_SD_writer.writeF32AsInt16(queueL.readBuffer(),queueR.readBuffer(),audio_settings.audio_block_samples); //stereo
      queueL.freeBuffer(); queueR.freeBuffer();

      //print a warning if there has been an SD writing hiccup
      if (PRINT_OVERRUN_WARNING) {
        if (queueL.getOverrun() || queueR.getOverrun() || i2s_in.get_isOutOfMemory()) {
          float blocksPerSecond = audio_settings.sample_rate_Hz / ((float)audio_settings.audio_block_samples);
          BOTH_SERIAL.print("SD Write Warning: there was a hiccup in the writing.  Approx Time (sec): ");
          BOTH_SERIAL.println( ((float)my_SD_writer.getNBlocksWritten()) / blocksPerSecond );
        }
      }

      //print timing information to help debug hiccups in the audio.  Are the writes fast enough?  Are there overruns?
      if (PRINT_FULL_SD_TIMING) {
        Serial.print("SD Write Status: "); 
        Serial.print(queueL.getOverrun()); //zero means no overrun
        Serial.print(", ");
        Serial.print(queueR.getOverrun()); //zero means no overrun
        Serial.print(", ");
        Serial.print(AudioMemoryUsageMax_F32());  //hopefully, is less than MAX_F32_BLOCKS
        //Serial.print(", ");
        //Serial.print(MAX_F32_BLOCKS);  // max possible memory allocation
        Serial.print(", ");
        Serial.println(i2s_in.get_isOutOfMemory());  //zero means i2s_input always had memory to work with.  Non-zero means it ran out at least once.
        
        //Now that we've read the flags, reset them.
        AudioMemoryUsageMaxReset_F32();
      }

      queueL.clearOverrun();
      queueR.clearOverrun();
      i2s_in.clear_isOutOfMemory();
    }
  } else {
    //no SD recording currently, so no SD action
  }
}

