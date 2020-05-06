#include "Decoder.h"
#include <iostream>

#include "dr_wav.h"

namespace rmixer
{
  
Decoder_WAV::Decoder_WAV() : pWav_(0), original_bps_(0), is_compressed_(false) {}

Decoder_WAV::~Decoder_WAV() { close(); }

bool Decoder_WAV::open(rutil::FileData &fd)
{
  return open((char*)fd.p, fd.len);
}

bool Decoder_WAV::open(const char* p, size_t len)
{
  drwav* dWav;
  int is_signed;
  close();
  if (p == 0 || len == 0)
    return false;
  pWav_ = dWav = drwav_open_memory(p, len);
  if (!pWav_) return false;

  // initialize soundinfo
  if (dWav->translatedFormatTag == DR_WAVE_FORMAT_IEEE_FLOAT)
  {
    is_signed = 2;
  }
  else if (dWav->bitsPerSample <= 8) {
    is_signed = 0;
  }
  else {
    is_signed = 1;
  }
  info_ = SoundInfo(is_signed, (uint8_t)dWav->bitsPerSample, (uint8_t)dWav->channels, dWav->sampleRate);
  original_bps_ = info_.bitsize;
  // if it is formatted as compressed (e.g. ADPCM), it cannot be opened as raw.
  // in that case, we force audio to open specified bit-per-sample.
  if (dWav->translatedFormatTag == DR_WAVE_FORMAT_ADPCM ||
      dWav->translatedFormatTag == DR_WAVE_FORMAT_DVI_ADPCM)
  {
    is_compressed_ = true;
  }
  return true;
}

void Decoder_WAV::close()
{
  if (pWav_)
  {
    drwav_close((drwav *)pWav_);
    pWav_ = 0;
  }
}

uint32_t Decoder_WAV::read_internal(char** p, bool read_raw)
{
  if (!pWav_)
    return 0;

  drwav* dWav = (drwav*)pWav_;
  uint32_t r = 0;

  // if is compressed, then it cannot be read by raw.
  if (is_compressed_ && read_raw)
  {
    if (info_.bitsize < 16)
      info_.bitsize = 16;
    else if (info_.bitsize < 32)
      info_.bitsize = 32;
    info_.is_signed = 1;
    return read_internal(p, false);
  }

  *p = (char*)malloc(GetByteFromFrame((uint32_t)dWav->totalPCMFrameCount, info_));
  if (read_raw)
  {
    r = (uint32_t)drwav_read_pcm_frames(dWav, dWav->totalPCMFrameCount, *p);
  }
  else
  {
    switch (info_.is_signed)
    {
    case 0:
      break;
    case 1:
      switch (info_.bitsize)
      {
      case 16:
        r = (uint32_t)drwav_read_pcm_frames_s16(dWav, dWav->totalPCMFrameCount, (int16_t*)*p);
        break;
      case 32:
        r = (uint32_t)drwav_read_pcm_frames_s32(dWav, dWav->totalPCMFrameCount, (int32_t*)*p);
        break;
      default:
        break;
      }
      break;
    case 2:
      switch (info_.bitsize)
      {
      case 32:
        r = (uint32_t)drwav_read_pcm_frames_f32(dWav, dWav->totalPCMFrameCount, (float*)*p);
        break;
        break;
      default:
        break;
      }
      break;
    default:
      break;
    }
  }

  if (r == 0)
  {
    free(*p);
    *p = 0;
  }

  return r / dWav->channels;
}

// @DEPRECIATED
uint32_t Decoder_WAV::readAsS32(char **p)
{
  drwav* dWav = (drwav*)pWav_;
  info_ = SoundInfo(1, 32, (uint8_t)dWav->channels, dWav->sampleRate);
  uint32_t r = (uint32_t)drwav_read_s32(dWav, dWav->totalPCMFrameCount * dWav->channels, (int32_t*)*p);
  return (uint32_t)dWav->totalPCMFrameCount;
}

}
