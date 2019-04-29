
/*
   Chip Audette, OpenAudio, Apr 2019

   MIT License.  Use at your own risk.
*/

#ifndef _AudioSDWriter_h
#define _AudioSDWriter_h

#include "SDWriter.h"
#include "AudioStream_F32.h"

//variables to control printing of warnings and timings and whatnot
#define PRINT_FULL_SD_TIMING 0    //set to 1 to print timing information of *every* write operation.  Great for logging to file.  Bad for real-time human reading.

//AudioSDWriter: A class to write data from audio blocks as part of the 
//   Teensy/Tympan audio processing paradigm.  The AudioSDWriter class is 
//   just a virtual Base class.  Use AudioSDWriter_F32 further down.
class AudioSDWriter {
  public:
    AudioSDWriter(void) {};
    enum class STATE { UNPREPARED = -1, STOPPED, RECORDING };
    STATE getState(void) {
      return current_SD_state;
    };
    enum class WriteDataType { INT16, FLOAT32 };
    void setNumWriteChannels(int n) {
      numWriteChannels = max(1, min(n, 2));  //can be 1 or 2
    }
    int getNumWriteChannels(void) {
      return numWriteChannels;
    }

    //virtual void prepareSDforRecording(void) = 0;
    //virtual int startRecording(void) = 0;
    //virtual int startRecording(char *) = 0;
    //virtual void stopRecording(void) = 0;

  protected:
    STATE current_SD_state = STATE::UNPREPARED;
    WriteDataType writeDataType = WriteDataType::INT16;
    int recording_count = 0;
    int numWriteChannels = 2;
};

//AudioSDWriter_F32: A class to write data from audio blocks as part
//   of the Teensy/Tympan audio processing paradigm.  For this class, the
//   audio is given as float32 and written as int16
class AudioSDWriter_F32 : public AudioSDWriter, public AudioStream_F32 {
  //GUI: inputs:2, outputs:0 //this line used for automatic generation of GUI node
  public:
    AudioSDWriter_F32(void) :
      AudioSDWriter(),
      AudioStream_F32(2, inputQueueArray)
        { setup(); }
    AudioSDWriter_F32(const AudioSettings_F32 &settings) :
      AudioSDWriter(),
      AudioStream_F32(2, inputQueueArray)
        { setup(); }
    AudioSDWriter_F32(const AudioSettings_F32 &settings, Print* _serial_ptr) :
      AudioSDWriter(),
      AudioStream_F32(2, inputQueueArray)
        { setup(_serial_ptr);  }
    AudioSDWriter_F32(const AudioSettings_F32 &settings, Print* _serial_ptr, const int _writeSizeBytes) :
      AudioSDWriter(),
      AudioStream_F32(2, inputQueueArray)
        { setup(_serial_ptr, _writeSizeBytes); }
    ~AudioSDWriter_F32(void) {
      stopRecording();
      delete buffSDWriterF32;
      delete buffSDWriterI16;
    }

    void setup(void) {
      setWriteDataType(WriteDataType::INT16, &Serial, DEFAULT_SDWRITE_BYTES);
    }
    void setup(Print *_serial_ptr) {
      setSerial(_serial_ptr);
      setWriteDataType(WriteDataType::INT16, _serial_ptr, DEFAULT_SDWRITE_BYTES);
    }
    void setup(Print *_serial_ptr, const int _writeSizeBytes) {
      setSerial(_serial_ptr);
      setWriteDataType(WriteDataType::INT16, _serial_ptr, _writeSizeBytes);
    }

    void setSerial(Print *_serial_ptr) {  serial_ptr = _serial_ptr;  }
    void setWriteDataType(WriteDataType type) {
      Print *serial_ptr = &Serial1;
      int write_nbytes = DEFAULT_SDWRITE_BYTES;

      //get info from previous objects
      if (buffSDWriterF32) {
        serial_ptr = buffSDWriterF32->getSerial();
        write_nbytes = buffSDWriterF32->getWriteSizeBytes();
      } else if (buffSDWriterI16) {
        serial_ptr = buffSDWriterI16->getSerial();
        write_nbytes = buffSDWriterI16->getWriteSizeBytes();
      }

      //make the full method call
      setWriteDataType(type, serial_ptr, write_nbytes);
    }
    void setWriteDataType(WriteDataType type, Print* serial_ptr, const int writeSizeBytes) {
      stopRecording();
      switch (type) {
        case (WriteDataType::INT16):
          writeDataType = type;
          if (buffSDWriterF32) delete buffSDWriterF32;
          if (!buffSDWriterI16) buffSDWriterI16 = new BufferedSDWriter_I16(serial_ptr, writeSizeBytes);
          break;
        case (WriteDataType::FLOAT32):
          writeDataType = type;
          if (buffSDWriterI16) delete buffSDWriterI16;
          if (!buffSDWriterF32) buffSDWriterF32 = new BufferedSDWriter_F32(serial_ptr, writeSizeBytes);
          break;
      }
    }
    void setWriteSizeBytes(const int n) {  //512Bytes is most efficient for SD
      if (buffSDWriterI16) {
        buffSDWriterI16->setWriteSizeBytes(n);
      } else if (buffSDWriterF32) {
        buffSDWriterF32->setWriteSizeBytes(n);
      }
    }
    int getWriteSizeBytes(void) {  //512Bytes is most efficient for SD
      if (buffSDWriterI16) {
        return buffSDWriterI16->getWriteSizeBytes();
      } else if (buffSDWriterF32) {
        return buffSDWriterF32->getWriteSizeBytes();
      }
    }

    void prepareSDforRecording(void) {
      if (current_SD_state == STATE::UNPREPARED) {
        if (buffSDWriterI16) {
          buffSDWriterI16->init(); //part of SDWriter, which is the base for BufferedSDWriter_I16
          if (PRINT_FULL_SD_TIMING) buffSDWriterI16->enablePrintElapsedWriteTime(); //for debugging.  make sure time is less than (audio_block_samples/sample_rate_Hz * 1e6) = 2900 usec for 128 samples at 44.1 kHz
        } else if (buffSDWriterF32) {
          buffSDWriterF32->init();
          if (PRINT_FULL_SD_TIMING) buffSDWriterF32->enablePrintElapsedWriteTime(); //for debugging.  make sure time is less than (audio_block_samples/sample_rate_Hz * 1e6) = 2900 usec for 128 samples at 44.1 kHz
        }
        current_SD_state = STATE::STOPPED;
      }
    }
    int startRecording(void) {
      int return_val = 0;
      if (current_SD_state == STATE::STOPPED) {
        recording_count++;
        if (recording_count < 100) {
          //make file name
          char fname[] = "RECORDxx.RAW";
          int tens = recording_count / 10;  //truncates
          fname[6] = tens + '0';  //stupid way to convert the number to a character
          int ones = recording_count - tens * 10;
          fname[7] = ones + '0';  //stupid way to convert the number to a character

          //open the file
          return_val = startRecording(fname);
        } else {
          if (serial_ptr) {
            serial_ptr->println("AudioSDWriter: start: Cannot do more than 99 files.");
          }
        }
      } else {
        if (serial_ptr) {
          serial_ptr->println("AudioSDWriter: start: not in correct state to start.");
        }
        return_val = -1;
      }
      return return_val;
    }
    int startRecording(char* fname) {
      int return_val = 0;
      if (current_SD_state == STATE::STOPPED) {
        if (open(fname)) {
          if (serial_ptr) {
            serial_ptr->print("AudioSDWriter: Opened ");
            serial_ptr->println(fname);
          }
          queueL.begin(); queueR.begin();
          current_SD_state = STATE::RECORDING;
        } else {
          if (serial_ptr) {
            serial_ptr->print("AudioSDWriter: start: Failed to open ");
            serial_ptr->println(fname);
          }
          return_val = -1;
        }
      } else {
        if (serial_ptr) {
          serial_ptr->println("AudioSDWriter: start: not in correct state to start.");
        }
        return_val = -1;
      }
      return return_val;
    }

    void stopRecording(void) {
      if (current_SD_state == STATE::RECORDING) {
        if (serial_ptr) {
          serial_ptr->println("stopRecording: Closing SD File...");
        }

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

    bool isFileOpen(void) {
      if (buffSDWriterI16) {
        return buffSDWriterI16->isFileOpen();
      } else if (buffSDWriterF32) {
        return buffSDWriterF32->isFileOpen();
      } else {
        return false;
      }
    }

    //this is what pulls data from the queues and sends to SD for writing.
    //should be invoked from loop(), not from an ISR
    int serviceSD(void) {
      if (numWriteChannels == 1) {
        return serviceSD_oneChan();
      } else if (numWriteChannels == 2) {
        return serviceSD_twoChan();
      } else {
        return 0;
      }
    }
    int serviceSD_oneChan(void) {
      int return_val = 0;
      //is the SD subsystem ready to write?
      if (isFileOpen()) {
        //if audio data is ready, write it to SD
        if (queueL.available()) {
          audio_block_f32_t *foo = queueL.getAudioBlock();
          writeOneChannel(foo->data, foo->length);
          queueL.freeBuffer();
          return_val = 1;

          //clear out right channel even if we're not processing it
          if (queueR.available()) {
            foo = queueR.getAudioBlock(); //do I actually need to get it?  Or can I just freeBuffer() it?
            queueR.freeBuffer();
          }
        }
      }
      return return_val;
    }
    int serviceSD_twoChan(void) {
      int return_val = 0;
      //is the SD subsystem ready to write?
      if (isFileOpen()) {
        if (queueL.available() && queueR.available()) {
          audio_block_f32_t *left = queueL.getAudioBlock(), *right = queueR.getAudioBlock();
          interleaveAndWrite(
            left->data,    //float32 array for left audio channel
            right->data,   //float32 array for right audio channel
            left->length); //number of samples in each channel
          queueL.freeBuffer(); queueR.freeBuffer();  //free up these blocks now that they are written
          return_val = 1;
        }
      }
      return return_val;
    }

    unsigned long getNBlocksWritten(void) {
      if (buffSDWriterI16) {
        return buffSDWriterI16->getNBlocksWritten();
      } else if (buffSDWriterF32) {
        return buffSDWriterF32->getNBlocksWritten();
      } else {
        return 0;
      }
    }
    void resetNBlocksWritten(void) {
      if (buffSDWriterI16) {
        buffSDWriterI16->resetNBlocksWritten();
      } else if (buffSDWriterF32) {
        buffSDWriterF32->resetNBlocksWritten();
      }
    }


    bool getQueueOverrun(void) {
      return (queueL.getOverrun() || queueR.getOverrun());
    }
    void clearQueueOverrun(void) {
      queueL.clearOverrun();
      queueR.clearOverrun();
    }

  protected:
    audio_block_f32_t *inputQueueArray[2]; //two input channels
    AudioRecordQueue_F32 queueL, queueR;
    BufferedSDWriter_I16 *buffSDWriterI16 = 0;
    BufferedSDWriter_F32 *buffSDWriterF32 = 0;
    Print *serial_ptr = &Serial;

    bool open(char *fname) {
      if (buffSDWriterI16) {
        return buffSDWriterI16->open(fname);
      } else if (buffSDWriterF32) {
        return buffSDWriterF32->open(fname);
      } else {
        return false;
      }
    }
    int interleaveAndWrite(float32_t *left, float32_t *right, const int &length) {
      if (buffSDWriterI16) {
        return buffSDWriterI16->interleaveAndWrite(left, right, length);
      } else if (buffSDWriterF32) {
        return buffSDWriterF32->interleaveAndWrite(left, right, length);
      } else {
        return 0;
      }
    }
    int writeOneChannel(float32_t *left, const int &length) {
      if (buffSDWriterI16) {
        return buffSDWriterI16->writeOneChannel(left, length);
      } else if (buffSDWriterF32) {
        return buffSDWriterF32->writeOneChannel(left, length);
      } else {
        return 0;
      }
    }
    int close(void) {
      if (buffSDWriterI16) {
        return buffSDWriterI16->close();
      } else if (buffSDWriterF32) {
        return buffSDWriterF32->close();
      } else {
        return 0;
      }
    }
};

#endif
