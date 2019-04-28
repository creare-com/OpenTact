
/*
   Chip Audette, OpenAudio, Mar 2018

   MIT License.  Use at your own risk.
*/

//variables to control printing of warnings and timings and whatnot
#define PRINT_OVERRUN_WARNING 1   //set to 1 to print a warning that the there's been a hiccup in the writing to the SD.
#define PRINT_FULL_SD_TIMING 0    //set to 1 to print timing information of *every* write operation.  Great for logging to file.  Bad for real-time human reading.

////////////////////////////////////////////////////////////

#ifndef _SDAudioWriter_h
#define _SDAudioWriter_h

//include <SdFat_Gre.h>       //originally from https://github.com/greiman/SdFat  but class names have been modified to prevent collisions with Teensy Audio/SD libraries
#include <Tympan_Library.h>  //for data types float32_t and int16_t and whatnot
#include "AudioStream_F32.h"
//#include <AudioStream.h>     //for AUDIO_BLOCK_SAMPLES

//some constants for the SDAudioWriter
const int DEFAULT_SDWRITE_BYTES = 512;  //minmum of 512 bytes is most efficient for SD
const uint64_t PRE_ALLOCATE_SIZE = 40ULL << 20;// Preallocate 40MB file.

//SDArrayWriter:  This is a class to make it easier to write blocks of bytes, ints, or floats
//  to the SD card.  It will write blocks of data of whatever the size, even if it is not
//  most efficient for the SD card.
//
//  To handle the interleaving of multiple channels and to handle conversion to the
//  desired write type (float32 -> int16) and to handle buffering so that the optimal
//  number of bytes are written at once, use one of the derived classes such as
//  SDArrayWriter_I16 or SDArrayWriter_F32
class SDArrayWriter
{
  public:
    SDArrayWriter() {};
    SDArrayWriter(Print* _serial_ptr) { setSerial(_serial_ptr); };
    ~SDArrayWriter() {
      if (isFileOpen()) {
        close();
      }
    }
    void setup(void) {
      init();
    };
    virtual void init() {
      if (!sd.begin()) sd.errorHalt(serial_ptr, "SDArrayWriter: begin failed");
    }

    //write Byte buffer...the lowest-level call upon which the others are built.
    //writing 512 is most efficient (ie 256 int16 or 128 float32
    virtual int writeBuffer(byte* buff, int nbytes) {
      if (file.isOpen()) {
        
        // write all audio bytes (512 bytes is most efficient)
        if (flagPrintElapsedWriteTime) { usec = 0; }
        file.write((byte *)buff, nbytes);
        nBlocksWritten++;
        
        //write elapsed time only to USB serial (because only that is fast enough)
        if (flagPrintElapsedWriteTime) { Serial.print("SD, us="); Serial.println(usec);  }
      }
      return 0;
    }
    
    //write Int16 buffer.
    virtual int writeBuffer(int16_t *buff, int nsamps) {
      writeBuffer((byte *)buff, nsamps * sizeof(buff[0]));
      return 0;
    }
    
    //write float32 buffer
    virtual int writeBuffer(float32_t *buff, int nsamps) {
      writeBuffer((byte *)buff, nsamps * sizeof(buff[0]));
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
    virtual void setSerial(Print *ptr) { serial_ptr = ptr; }

  protected:
    //SdFatSdio sd; //slower
    SdFatSdioEX sd; //faster
    SdFile_Gre file;
    boolean flagPrintElapsedWriteTime = false;
    elapsedMicros usec;
    unsigned long nBlocksWritten = 0;
    Print* serial_ptr = &Serial;

};

//SDArrayWriter_16:  This is a drived class from SDArrayWriter.  This class assumes that
//  you want to write Int16 data to the SD card.  You may give this class data that is
//  either Int16 or Float32.  This class will also handle interleaving of two input
//  channels.  This class will also buffer the data until the optimal (or desired) number
//  of samples have been accumulated, which makes the SD writing more efficient.
class SDArrayWriter_I16 : public SDArrayWriter
{
  public:
    SDArrayWriter_I16() : SDArrayWriter() { 
      setWriteSizeSamples(DEFAULT_SDWRITE_BYTES / 2); //2 bytes per I16 sample
      };
    SDArrayWriter_I16(Print* _serial_ptr) : SDArrayWriter(_serial_ptr) {
      setWriteSizeSamples(DEFAULT_SDWRITE_BYTES / 2); //2 bytes per I16 sample
    };
    SDArrayWriter_I16(Print* _serial_ptr, const int _writeSizeBytes) : SDArrayWriter(_serial_ptr) {
      setWriteSizeSamples(_writeSizeBytes / 2); //2 bytes per I16 sample
    };
    ~SDArrayWriter_I16(void) {
      delete write_buffer;
    }

    void setWriteSizeSamples(const int _writeSizeSamples) {
      //ensure even number greater than 0
      writeSizeSamples = max(2,2*int(_writeSizeSamples/2));

      //create write buffer
      if (write_buffer != 0) { delete write_buffer; } //delete the old buffer
      write_buffer = new int16_t[writeSizeSamples];

      //reset the buffer index
      buffer_ind = 0;
    }
    int getWriteSizeSamples(void) { return writeSizeSamples; }

    virtual int interleaveAndWrite(int16_t *chan1, int16_t *chan2, int nsamps) {
      if (write_buffer == 0) return -1;
      int return_val = 0;

      //interleave the data and write whenever the write buffer is full
      for (int Isamp = 0; Isamp < nsamps; Isamp++) {
        write_buffer[buffer_ind++] = chan1[Isamp];
        write_buffer[buffer_ind++] = chan2[Isamp];

        //do we have enough data to write our block to SD?
        if (buffer_ind >= writeSizeSamples) {
          return_val = writeBuffer((byte *)write_buffer,writeSizeSamples*sizeof(write_buffer[0]));
          buffer_ind = 0;  //jump back to beginning of buffer
        }
      }
      return return_val;
    }

    virtual int interleaveAndWrite(float32_t *chan1, float32_t *chan2, int nsamps) {
      if (write_buffer == 0) return -1;
      int return_val = 0;

      //interleave the data and write whenever the write buffer is full
      for (int Isamp = 0; Isamp < nsamps; Isamp++) {
        //convert the F32 to Int16 and interleave
        write_buffer[buffer_ind++] = (int16_t)(chan1[Isamp] * 32767.0);
        write_buffer[buffer_ind++] = (int16_t)(chan2[Isamp] * 32767.0);

        //do we have enough data to write our block to SD?
        if (buffer_ind >= writeSizeSamples) {
          return_val = writeBuffer((byte *)write_buffer,writeSizeSamples*sizeof(write_buffer[0]));
          buffer_ind = 0;  //jump back to beginning of buffer
        }
      }
      return return_val;
    }

    //write one channel of int16 as int16
    virtual int writeOneChannel(int16_t *chan1, int nsamps) {
      if (write_buffer == 0) return -1;
      int return_val = 0;

      if (nsamps == writeSizeSamples) {
        //special case where everything is the right size...it'll be fast!
        return writeBuffer((byte *)chan1, writeSizeSamples*sizeof(chan1[0]));
      } else {
        //do the buffering and write when the buffer is full
       
        for (int Isamp = 0; Isamp < nsamps; Isamp++) {
          //convert the F32 to Int16 and interleave
          write_buffer[buffer_ind++] = chan1[Isamp];
  
          //do we have enough data to write our block to SD?
          if (buffer_ind >= writeSizeSamples) {
            return_val = writeBuffer((byte *)write_buffer,writeSizeSamples*sizeof(write_buffer[0]));
            buffer_ind = 0;  //jump back to beginning of buffer
          }     
        }
        return return_val;
      }
      return return_val;
    }

    //write one channel of float32 as int16
    virtual int writeOneChannel(float32_t *chan1, int nsamps) {
     if (write_buffer == 0) return -1;
      int return_val = 0;

      //interleave the data and write whenever the write buffer is full
      for (int Isamp = 0; Isamp < nsamps; Isamp++) {
        //convert the F32 to Int16 and interleave
        write_buffer[buffer_ind++] = (int16_t)(chan1[Isamp] * 32767.0);

        //do we have enough data to write our block to SD?
        if (buffer_ind >= writeSizeSamples) {
          return_val = writeBuffer((byte *)write_buffer,writeSizeSamples*sizeof(write_buffer[0]));
          buffer_ind = 0;  //jump back to beginning of buffer
        }
      }
      return return_val;
    }
    
  protected:
    int writeSizeSamples = 0;
    int16_t* write_buffer = 0;
    int buffer_ind = 0;
};

//SDAudioWriter_F32:  This is a drived class from SDAudioWriter.  This class assumes that
//  you want to write float32 data to the SD card.  This class will handle interleaving 
//  of two input channels.  This class will also buffer the data until the optimal (or 
//  desired) number of samples have been accumulated, which makes the SD writing more efficient.
class SDArrayWriter_F32 : public SDArrayWriter
{
  public:
    SDArrayWriter_F32() : SDArrayWriter() { 
      setWriteSizeSamples(DEFAULT_SDWRITE_BYTES / 4); //4 bytes per float32 sample
    };
    SDArrayWriter_F32(Print* _serial_ptr) : SDArrayWriter(_serial_ptr) {
      setWriteSizeSamples(DEFAULT_SDWRITE_BYTES / 4); //4 bytes per float32 sample
    };
    SDArrayWriter_F32(Print* _serial_ptr, const int _writeSizeBytes) : SDArrayWriter(_serial_ptr) {
      setWriteSizeSamples(_writeSizeBytes / 4); //4 bytes per float32 sample
    };
    ~SDArrayWriter_F32(void) {
      delete write_buffer;
    }
    
    void setWriteSizeSamples(const int _writeSizeSamples) {
      //ensure even number greater than 0
      writeSizeSamples = max(2,2*int(_writeSizeSamples/2));

      //create write buffer
      if (write_buffer != 0) { delete write_buffer; } //delete the old buffer
      write_buffer = new float32_t[writeSizeSamples];

      //reset the buffer index
      buffer_ind = 0;
    }
    int getWriteSizeSamples(void) { return writeSizeSamples; }

    //write two channels of float32 as float32
    virtual int interleaveAndWrite(float32_t *chan1, float32_t *chan2, int nsamps) {
      if (write_buffer == 0) return -1;
      int return_val = 0;

      //interleave the data and write whenever the write buffer is full
      for (int Isamp = 0; Isamp < nsamps; Isamp++) {
        write_buffer[buffer_ind++] = chan1[Isamp];
        write_buffer[buffer_ind++] = chan2[Isamp];

        //do we have enough data to write our block to SD?
        if (buffer_ind >= writeSizeSamples) {
          return_val = writeBuffer((byte *)write_buffer,writeSizeSamples*sizeof(write_buffer[0]));
          buffer_ind = 0;  //jump back to beginning of buffer
        }
      }
      return return_val;
    }

    //write one channel of float32 as float32
    virtual int writeOneChannel(float32_t *chan1, int nsamps) {
      if (write_buffer == 0) return -1;
      int return_val = 0;

      if (nsamps == writeSizeSamples) {
        //special case where everything is the right size...it'll be fast!
        return writeBuffer((byte *)chan1, writeSizeSamples*sizeof(chan1[0]));
      } else {
        //do the buffering and write when the buffer is full
       
        for (int Isamp = 0; Isamp < nsamps; Isamp++) {
          //convert the F32 to Int16 and interleave
          write_buffer[buffer_ind++] = chan1[Isamp];
  
          //do we have enough data to write our block to SD?
          if (buffer_ind >= writeSizeSamples) {
            return_val = writeBuffer((byte *)write_buffer,writeSizeSamples*sizeof(write_buffer[0]));
            buffer_ind = 0;  //jump back to beginning of buffer
          }     
        }
        return return_val;
      }
      return return_val;
    }
    
  protected:
    int writeSizeSamples = 0;
    float32_t *write_buffer = 0;
    int buffer_ind = 0;
};

#endif

// /////////////////////////////////////////////////////////////

class SDAudioWriter {
  public:
    SDAudioWriter(void) {};    
    enum class STATE { UNPREPARED = -1, STOPPED, RECORDING };
    STATE getState(void) { return current_SD_state; };
    
    virtual void prepareSDforRecording(void) = 0;
    virtual int startRecording(void) = 0;
    virtual int startRecording(char *) = 0;
    virtual void stopRecording(void) = 0;
     
  protected:
    STATE current_SD_state = STATE::UNPREPARED;
    int recording_count = 0;

};

//SDAudioWriter_F32asI16: A class to write data from audio blocks as part
//   of the Teensy/Tympan audio processing paradigm.  For this class, the
//   audio is given as float32 and written as int16
class SDAudioWriter_F32asI16 : public SDArrayWriter_I16 , public SDAudioWriter, public AudioStream_F32 {
  //GUI: inputs:2, outputs:0 //this line used for automatic generation of GUI node
  public:
    SDAudioWriter_F32asI16(void) :
        SDArrayWriter_I16(), 
        AudioStream_F32(2, inputQueueArray)
          { setupQueues();}
    SDAudioWriter_F32asI16(const AudioSettings_F32 &settings) :
        SDArrayWriter_I16(), 
        AudioStream_F32(2, inputQueueArray)
          { setupQueues();}
    SDAudioWriter_F32asI16(const AudioSettings_F32 &settings, Print* _serial_ptr) :
        SDArrayWriter_I16(_serial_ptr),
        AudioStream_F32(2, inputQueueArray)
          {setupQueues(); }
    SDAudioWriter_F32asI16(const AudioSettings_F32 &settings, Print* _serial_ptr, const int _writeSizeBytes) : 
        SDArrayWriter_I16(_serial_ptr, _writeSizeBytes),
        AudioStream_F32(2, inputQueueArray)
          { setupQueues(); }

    virtual void setupQueues(void) { } //nothing to do?

    virtual void prepareSDforRecording(void) {
      if (current_SD_state == STATE::UNPREPARED) {
        init(); //part of SDArrayWriter
        if (PRINT_FULL_SD_TIMING) enablePrintElapsedWriteTime(); //for debugging.  make sure time is less than (audio_block_samples/sample_rate_Hz * 1e6) = 2900 usec for 128 samples at 44.1 kHz
        current_SD_state = STATE::STOPPED;
      }
    }

    virtual int startRecording(void) {
      int return_val = 0;
      if (current_SD_state == STATE::STOPPED) {
        recording_count++; 
        if (recording_count < 100) {
          //make file name
          char fname[] = "RECORDxx.RAW";
          int tens = recording_count / 10;  //truncates
          fname[6] = tens + '0';  //stupid way to convert the number to a character
          int ones = recording_count - tens*10;
          fname[7] = ones + '0';  //stupid way to convert the number to a character

          //open the file
          return_val = startRecording(fname);
        } else {
          serial_ptr->println("SDAudioWriter: start: Cannot do more than 99 files.");
        }
      } else {
        serial_ptr->println("SDAudioWriter: start: not in correct state to start.");
        return_val = -1;
      }
      return return_val;
    }
    
    virtual int startRecording(char* fname) {
      int return_val = 0;
      if (current_SD_state == STATE::STOPPED) {
        if (open(fname)) {
          serial_ptr->print("SDAudioWriter: Opened "); serial_ptr->println(fname);
          queueL.begin(); queueR.begin();
          current_SD_state = STATE::RECORDING;
        } else {
          serial_ptr->print("SDAudioWriter: start: Failed to open "); serial_ptr->println(fname);
          return_val = -1;
        }
      } else {
        serial_ptr->println("SDAudioWriter: start: not in correct state to start.");
        return_val = -1;
      }
      return return_val;
    }

    virtual void stopRecording(void) {
      if (current_SD_state == STATE::RECORDING) {
        serial_ptr->println("stopRecording: Closing SD File...");
      
        //close the file
        close(); 
        current_SD_state = STATE::STOPPED;
  
        //stop the queues so that they stop accumulating data
        queueL.end();  queueR.end();
      }
    }

    //update is called by the Audio processing ISR.  This update function should
    //only service the recording queues so as to buffer the audio data.
    //The acutal SD writing should occur in the loop() as invoked by a service routine
    void update(void) {
      queueL.update(receiveReadOnly_f32(0));
      queueR.update(receiveReadOnly_f32(1));
    }
    AudioRecordQueue_F32 queueL, queueR;
    
  protected:
    audio_block_f32_t *inputQueueArray[2]; //two input channels
};



