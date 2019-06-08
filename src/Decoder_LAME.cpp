#include "Decoder.h"
#include "Error.h"

#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"

namespace rhythmus
{

/*
 * use s16 data in here.
 */

constexpr auto kMP3DefaultPCMBufferSize = 1024 * 1024 * 1u;

Decoder_LAME::Decoder_LAME(Sound &s)
  : Decoder(s), pContext_(0)
{
}

bool Decoder_LAME::open(rutil::FileData &fd)
{
  close();
  pContext_ = (void*)malloc(sizeof(drmp3));
  drmp3 &mp3 = *(drmp3*)pContext_;
  if (!drmp3_init_memory(&mp3, fd.GetPtr(), fd.GetFileSize(), 0))
    return false;

  return true;
}

void Decoder_LAME::close()
{
  if (pContext_)
  {
    free(pContext_);
    pContext_ = 0;
  }
}

uint32_t Decoder_LAME::read()
{
  if (!pContext_)
    return 0;

  drmp3 &mp3 = *(drmp3*)pContext_;
  uint32_t framecount = 0;
  uint32_t readframecount = 0;
  size_t current_buffer_bytesize = kMP3DefaultPCMBufferSize;
  int16_t *buffer = (int16_t*)malloc(current_buffer_bytesize);
  constexpr auto kBps = 2;  /* Byte Per Sample (16bit) */
  const uint32_t frames_to_read_at_once = current_buffer_bytesize / kBps / mp3.channels;

  do {
    readframecount = drmp3_read_pcm_frames_s16(&mp3, frames_to_read_at_once, buffer + framecount * mp3.channels);
    framecount += readframecount;
    // check is_realloc_necessary
    if ((framecount + frames_to_read_at_once) * kBps * mp3.channels > current_buffer_bytesize)
    {
      current_buffer_bytesize *= 2;
      buffer = (int16_t*)realloc(buffer, current_buffer_bytesize);
      ASSERT(buffer);
    }
  } while (readframecount > 0);

  // - done -
  sound().Set(16, mp3.channels, framecount, mp3.sampleRate, buffer);

  return sound().get_total_byte();
}

}