

/*
   Chip Audette, OpenAudio, Apr 2019

   MIT License.  Use at your own risk.
*/


#ifndef _SDWriter_h
#define _SDWriter_h

#include <SdFat_Gre.h>       //originally from https://github.com/greiman/SdFat  but class names have been modified to prevent collisions with Teensy Audio/SD libraries
#include <Print.h>

//some constants for the AudioSDWriter
const int DEFAULT_SDWRITE_BYTES = 512;  //minmum of 512 bytes is most efficient for SD.  Only used for binary writes
const uint64_t PRE_ALLOCATE_SIZE = 40ULL << 20;// Preallocate 40MB file.

//SDWriter:  This is a class to make it easier to write blocks of bytes, ints, or floats
//  to the SD card.  It will write blocks of data of whatever the size, even if it is not
//  most efficient for the SD card.
//
//  To handle the interleaving of multiple channels and to handle conversion to the
//  desired write type (float32 -> int16) and to handle buffering so that the optimal
//  number of bytes are written at once, use one of the derived classes such as
//  BufferedSDWriter_I16 or BufferedSDWriter_F32
class SDWriter : public Print
{
  public:
    SDWriter() {};
    SDWriter(Print* _serial_ptr) {
      setSerial(_serial_ptr);
    };
    virtual ~SDWriter() {
      if (isFileOpen()) {
        close();
      }
    }

    void setup(void) {
      init();
    }
    virtual void init() {
      if (!sd.begin()) sd.errorHalt(serial_ptr, "SDWriter: begin failed");
    }

    bool open(char *fname) {
      if (sd.exists(fname)) {  //maybe this isn't necessary when using the O_TRUNC flag below
        // The SD library writes new data to the end of the
        // file, so to start a new recording, the old file
        // must be deleted before new data is written.
        sd.remove(fname);
      }

#if 1
      file.open(fname, O_RDWR | O_CREAT | O_TRUNC);
#else
      file.createContiguous(fname, PRE_ALLOCATE_SIZE);
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

    //This "write" is for compatibility with the Print interface.  Writing one
    //byte at a time is EXTREMELY inefficient and shouldn't be done
    virtual size_t write(uint8_t foo)  {
      size_t return_val = 0;
      if (file.isOpen()) {

        // write all audio bytes (512 bytes is most efficient)
        if (flagPrintElapsedWriteTime) {
          usec = 0;
        }
        file.write((byte *) (&foo), 1); //write one value
        return_val = 1;

        //write elapsed time only to USB serial (because only that is fast enough)
        if (flagPrintElapsedWriteTime) {
          Serial.print("SD, us=");
          Serial.println(usec);
        }
      }
      return return_val;
    }

    //write Byte buffer...the lowest-level call upon which the others are built.
    //writing 512 is most efficient (ie 256 int16 or 128 float32
    virtual size_t write(const uint8_t *buff, int nbytes) {
      size_t return_val = 0;
      if (file.isOpen()) {

        // write all audio bytes (512 bytes is most efficient)
        if (flagPrintElapsedWriteTime) {
          usec = 0;
        }
        file.write((byte *)buff, nbytes);
        return_val = nbytes;
        nBlocksWritten++;

        //write elapsed time only to USB serial (because only that is fast enough)
        if (flagPrintElapsedWriteTime) {
          Serial.print("SD, us=");
          Serial.println(usec);
        }
      }
      return return_val;
    }

    //write Int16 buffer.
    virtual size_t write(int16_t *buff, int nsamps) {
      return write((const uint8_t *)buff, nsamps * sizeof(buff[0]));
    }

    //write float32 buffer
    virtual size_t write(float32_t *buff, int nsamps) {
      return write((const uint8_t *)buff, nsamps * sizeof(buff[0]));
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
    virtual void setSerial(Print *ptr) {
      serial_ptr = ptr;
    }
    virtual Print* getSerial(void) {
      return serial_ptr;
    };

  protected:
    //SdFatSdio sd; //slower
    SdFatSdioEX sd; //faster
    SdFile_Gre file;
    boolean flagPrintElapsedWriteTime = false;
    elapsedMicros usec;
    unsigned long nBlocksWritten = 0;
    Print* serial_ptr = &Serial;
    //WriteDataType writeDataType = WriteDataType::INT16;

};

//BufferedSDWriter_I16:  This is a drived class from SDWriter.  This class assumes that
//  you want to write Int16 data to the SD card.  You may give this class data that is
//  either Int16 or Float32.  This class will also handle interleaving of two input
//  channels.  This class will also buffer the data until the optimal (or desired) number
//  of samples have been accumulated, which makes the SD writing more efficient.
class BufferedSDWriter_I16 : public SDWriter
{
  public:
    BufferedSDWriter_I16() : SDWriter() {
      setWriteSizeBytes(DEFAULT_SDWRITE_BYTES);
    };
    BufferedSDWriter_I16(Print* _serial_ptr) : SDWriter(_serial_ptr) {
      setWriteSizeBytes(DEFAULT_SDWRITE_BYTES );
    };
    BufferedSDWriter_I16(Print* _serial_ptr, const int _writeSizeBytes) : SDWriter(_serial_ptr) {
      setWriteSizeBytes(_writeSizeBytes);
    };
    ~BufferedSDWriter_I16(void) {
      delete write_buffer;
    }

    void setWriteSizeBytes(const int _writeSizeBytes) {
      setWriteSizeSamples(_writeSizeBytes / nBytesPerSample);
    }
    void setWriteSizeSamples(const int _writeSizeSamples) {
      //ensure even number greater than 0
      writeSizeSamples = max(2, 2 * int(_writeSizeSamples / 2));

      //create write buffer
      if (write_buffer != 0) {
        delete write_buffer;  //delete the old buffer
      }
      write_buffer = new int16_t[writeSizeSamples];

      //reset the buffer index
      buffer_ind = 0;
    }
    int getWriteSizeBytes(void) {
      return (getWriteSizeSamples() * nBytesPerSample);
    }
    int getWriteSizeSamples(void) {
      return writeSizeSamples;
    }

    virtual int interleaveAndWrite(int16_t *chan1, int16_t *chan2, int nsamps) {
      if (write_buffer == 0) return -1;
      int return_val = 0;

      //interleave the data and write whenever the write buffer is full
      for (int Isamp = 0; Isamp < nsamps; Isamp++) {
        write_buffer[buffer_ind++] = chan1[Isamp];
        write_buffer[buffer_ind++] = chan2[Isamp];

        //do we have enough data to write our block to SD?
        if (buffer_ind >= writeSizeSamples) {
          return_val = write((byte *)write_buffer, writeSizeSamples * sizeof(write_buffer[0]));
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
          return_val = write((byte *)write_buffer, writeSizeSamples * sizeof(write_buffer[0]));
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
        return write((byte *)chan1, writeSizeSamples * sizeof(chan1[0]));
      } else {
        //do the buffering and write when the buffer is full

        for (int Isamp = 0; Isamp < nsamps; Isamp++) {
          //convert the F32 to Int16 and interleave
          write_buffer[buffer_ind++] = chan1[Isamp];

          //do we have enough data to write our block to SD?
          if (buffer_ind >= writeSizeSamples) {
            return_val = write((byte *)write_buffer, writeSizeSamples * sizeof(write_buffer[0]));
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
          return_val = write((byte *)write_buffer, writeSizeSamples * sizeof(write_buffer[0]));
          buffer_ind = 0;  //jump back to beginning of buffer
        }
      }
      return return_val;
    }

  protected:
    int writeSizeSamples = 0;
    int16_t* write_buffer = 0;
    int buffer_ind = 0;
    const int nBytesPerSample = 2;
};

//BufferedSDWriter_F32:  This is a drived class from SDWriter.  This class assumes that
//  you want to write float32 data to the SD card.  This class will handle interleaving
//  of two input channels.  This class will also buffer the data until the optimal (or
//  desired) number of samples have been accumulated, which makes the SD writing more efficient.
class BufferedSDWriter_F32 : public SDWriter
{
  public:
    BufferedSDWriter_F32() : SDWriter() {
      setWriteSizeBytes(DEFAULT_SDWRITE_BYTES);
    };
    BufferedSDWriter_F32(Print* _serial_ptr) : SDWriter(_serial_ptr) {
      setWriteSizeBytes(DEFAULT_SDWRITE_BYTES);
    };
    BufferedSDWriter_F32(Print* _serial_ptr, const int _writeSizeBytes) : SDWriter(_serial_ptr) {
      setWriteSizeBytes(DEFAULT_SDWRITE_BYTES);
    };
    ~BufferedSDWriter_F32(void) {
      delete write_buffer;
    }

    void setWriteSizeBytes(const int _writeSizeBytes) {
      setWriteSizeSamples( _writeSizeBytes * nBytesPerSample);
    }
    void setWriteSizeSamples(const int _writeSizeSamples) {
      //ensure even number greater than 0
      writeSizeSamples = max(2, 2 * int(_writeSizeSamples / nBytesPerSample) );

      //create write buffer
      if (write_buffer != 0) {
        delete write_buffer;  //delete the old buffer
      }
      write_buffer = new float32_t[writeSizeSamples];

      //reset the buffer index
      buffer_ind = 0;
    }
    int getWriteSizeBytes(void) {
      return (getWriteSizeSamples() * nBytesPerSample);
    }
    int getWriteSizeSamples(void) {
      return writeSizeSamples;
    }

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
          return_val = write((byte *)write_buffer, writeSizeSamples * sizeof(write_buffer[0]));
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
        return write((byte *)chan1, writeSizeSamples * sizeof(chan1[0]));
      } else {
        //do the buffering and write when the buffer is full

        for (int Isamp = 0; Isamp < nsamps; Isamp++) {
          //convert the F32 to Int16 and interleave
          write_buffer[buffer_ind++] = chan1[Isamp];

          //do we have enough data to write our block to SD?
          if (buffer_ind >= writeSizeSamples) {
            return_val = write((byte *)write_buffer, writeSizeSamples * sizeof(write_buffer[0]));
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
    const int nBytesPerSample = 4;
};

#endif

