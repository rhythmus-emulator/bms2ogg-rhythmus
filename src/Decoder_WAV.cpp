#include "Decoder.h"
#include <iostream>

// implementation is in Sampler.cpp
#include "dr_wav.h"

namespace rmixer
{

Decoder_WAV::Decoder_WAV() : pWav_(0) {}

bool Decoder_WAV::open(rutil::FileData &fd)
{
  return open((char*)fd.p, fd.len);
}

bool Decoder_WAV::open(const char* p, size_t len)
{
  close();
  if (p == 0 || len == 0)
    return false;
  pWav_ = drwav_open_memory(p, len);
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

uint32_t Decoder_WAV::read(char** p)
{
  if (!pWav_)
    return 0;

  drwav* dWav = (drwav*)pWav_;
  uint32_t r = 0;
  info_ = SoundInfo(dWav->bitsPerSample, dWav->channels, dWav->sampleRate);
  *p = (char*)malloc(GetByteFromFrame(dWav->totalPCMFrameCount, info_));
  /** if not PCM, then use custom reading method (read as 16bit sound) */
  if (dWav->translatedFormatTag == DR_WAVE_FORMAT_PCM)
  {
    r = (uint32_t)drwav_read(dWav, dWav->totalPCMFrameCount * dWav->channels, *p);
  }
  else
  {
    r = (uint32_t)drwav_read_s16(dWav, dWav->totalPCMFrameCount * dWav->channels, (int16_t*)*p);
  }
  return r;
}

uint32_t Decoder_WAV::readAsS32(char **p)
{
  drwav* dWav = (drwav*)pWav_;
  info_ = SoundInfo(32, dWav->channels, dWav->sampleRate);
  uint32_t r = (uint32_t)drwav_read_s32(dWav, dWav->totalPCMFrameCount * dWav->channels, (int32_t*)*p);
  return r;
}

}