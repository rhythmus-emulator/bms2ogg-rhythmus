#include "Decoder.h"
#include <iostream>

// implementation is in Sampler.cpp
#include "dr_wav.h"

namespace rmixer
{

Decoder_WAV::Decoder_WAV(Sound &s) : Decoder(s), pWav_(0) {}

bool Decoder_WAV::open(rutil::FileData &fd)
{
  close();
  if (fd.p == 0 || fd.len == 0)
    return false;
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
  if (!pWav_)
    return 0;

  drwav* dWav = (drwav*)pWav_;
  uint32_t r = 0;
  /** if not PCM, then use custom reading method (read as 16bit sound) */
  if (dWav->translatedFormatTag == DR_WAVE_FORMAT_PCM)
  {
    sound().Set(dWav->bitsPerSample, dWav->channels, dWav->totalPCMFrameCount, dWav->sampleRate);
    r = (uint32_t)drwav_read(dWav, dWav->totalPCMFrameCount * dWav->channels, sound().ptr());
  }
  else
  {
    sound().Set(16, dWav->channels, dWav->totalPCMFrameCount, dWav->sampleRate);
    r = (uint32_t)drwav_read_s16(dWav, dWav->totalPCMFrameCount * dWav->channels, (int16_t*)sound().ptr());
  }
  return r;
}

uint32_t Decoder_WAV::readAsS32()
{
  drwav* dWav = (drwav*)pWav_;
  sound().Set(32, dWav->channels, dWav->totalPCMFrameCount, dWav->sampleRate);
  uint32_t r = (uint32_t)drwav_read_s32(dWav, dWav->totalPCMFrameCount * dWav->channels, (int32_t*)sound().ptr());
  return r;
}

}