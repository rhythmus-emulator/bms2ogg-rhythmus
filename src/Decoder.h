#ifndef RENCODER_DECODER_H
#define RENCODER_DECODER_H

#include <stdint.h>
#include "rutil.h"
#include "Sound.h"

namespace rhythmus
{

class Decoder
{
public:
  Decoder(Sound &s);
  ~Decoder();
  virtual bool open(rutil::FileData &fd) = 0;
  virtual void close();
  virtual uint32_t read() = 0;
  virtual uint32_t readAsS32() = 0;
  Sound& sound();
private:
  Sound *s_;
};

class Decoder_WAV : public Decoder
{
public:
  Decoder_WAV(Sound &s);
  virtual bool open(rutil::FileData &fd);
  virtual void close();
  virtual uint32_t read();
  virtual uint32_t readAsS32();
private:
  void* pWav_;
};

}

#endif