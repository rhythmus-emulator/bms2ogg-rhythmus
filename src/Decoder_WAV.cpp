#include "Decoder.h"
#include <iostream>

// implementation is in Sampler.cpp
#include "dr_wav.h"

namespace rhythmus
{

Decoder_WAV::Decoder_WAV(Sound &s) : Decoder(s) {}

bool Decoder_WAV::open(rutil::FileData &fd)
{
  pWav_ = drwav_open_memory(fd.p, fd.len);
  return pWav_ != 0;
}

void Decoder_WAV::close()
{
  if (pWav_)
  {
    drwav_close((drwav *)pWav_);
    pWav_ = 0;
  }
}

uint32_t Decoder_WAV::read()
{
  drwav* dWav = (drwav*)pWav_;
  sound().Set(dWav->bitsPerSample, dWav->channels, dWav->totalPCMFrameCount, dWav->sampleRate);
  uint32_t r = (uint32_t)drwav_read(dWav, dWav->totalPCMFrameCount, sound().ptr());
  return r;
}

uint32_t Decoder_WAV::readAsS32()
{
  drwav* dWav = (drwav*)pWav_;
  sound().Set(32, dWav->channels, dWav->totalPCMFrameCount, dWav->sampleRate);
  uint32_t r = (uint32_t)drwav_read_s32(dWav, dWav->totalPCMFrameCount, (int32_t*)sound().ptr());
  return r;
}

}