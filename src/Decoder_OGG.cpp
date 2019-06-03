#include "Decoder.h"
#include "Error.h"

/** https://svn.xiph.org/trunk/vorbis/examples/decoder_example.c */

namespace rhythmus
{

constexpr auto kOGGDecodeBufferSize = 4096u;
constexpr auto kOGGDefaultPCMBufferSize = 1024 * 1024 * 1u;  /* default allocating memory size for PCM decoding */

Decoder_OGG::Decoder_OGG(Sound &s) : Decoder(s) {}

bool Decoder_OGG::open(rutil::FileData &fd)
{
  ogg_sync_init(&oy);
  buffer = ogg_sync_buffer(&oy, kOGGDecodeBufferSize);
  bytes = fd.Read((uint8_t*)buffer, kOGGDecodeBufferSize);
  ogg_sync_wrote(&oy, bytes);
  if (ogg_sync_pageout(&oy, &og) != 1)
    return false;

  ogg_stream_init(&os, ogg_page_serialno(&og));
  vorbis_info_init(&vi);
  vorbis_comment_init(&vc);
  if (ogg_stream_pagein(&os, &og) < 0)
    return false;
  if (ogg_stream_packetout(&os, &op) != 1)
    return false;
  if (vorbis_synthesis_headerin(&vi, &vc, &op) < 0)
    return false;

  int i = 0;
  while (i < 2) {
    while (i < 2) {
      int result = ogg_sync_pageout(&oy, &og);
      if (result == 0) break; /* Need more data */
      if (result == 1) {
        ogg_stream_pagein(&os, &og);
        while (i < 2) {
          result = ogg_stream_packetout(&os, &op);
          if (result == 0) break;
          if (result < 0)
            return false;
          result = vorbis_synthesis_headerin(&vi, &vc, &op);
          if (result < 0)
            return false;
          i++;
        }
      }
    }
    buffer = ogg_sync_buffer(&oy, kOGGDecodeBufferSize);
    bytes = fd.Read((uint8_t*)buffer, kOGGDecodeBufferSize);
    if (bytes == 0 && i < 2) /* end of file before scanning all vorbis headers */
      return false;
    ogg_sync_wrote(&oy, bytes);
  }

  fdd = fd;
  return true;
}

void Decoder_OGG::close()
{
  ogg_stream_clear(&os);
  vorbis_comment_clear(&vc);
  vorbis_info_clear(&vi);
}

uint32_t Decoder_OGG::read()
{
  constexpr size_t byte_per_sample = 2;
  int eos = 0;
  int result = 0;
  int convframesize = kOGGDecodeBufferSize / byte_per_sample / vi.channels;
  ogg_int16_t convbuffer[kOGGDecodeBufferSize];
  int sample_offset = 0;
  size_t pcm_buffer_size = kOGGDefaultPCMBufferSize;
  char* pcm_buffer = (char*)malloc(pcm_buffer_size);
  //sound().Set(16, vi.channels, sample_total_count, vi.rate);

  if (vorbis_synthesis_init(&vd, &vi) == 0)
  {
    vorbis_block_init(&vd, &vb);

    while (!eos) {
      while (!eos) {
        result = ogg_sync_pageout(&oy, &og);
        if (result == 0) break; /* need more data */
        else if (result < 0) {
          // corrupt bitstream data
        }
        else {
          ogg_stream_pagein(&os, &og);
          while (1) {
            result = ogg_stream_packetout(&os, &op);
            if (result == 0) break; /* need more data */
            if (result < 0) {
              // missing or corrupt data (already checked above)
            }
            else {
              float **pcm;
              int frames;
              if (vorbis_synthesis(&vb, &op) == 0)
                vorbis_synthesis_blockin(&vd, &vb);

              while ((frames = vorbis_synthesis_pcmout(&vd, &pcm)) > 0) {
                int j;
                int clipflag = 0;
                int bout = (frames < convframesize ? frames : convframesize);
                for (int i = 0; i < vi.channels; i++) {
                  ogg_int16_t *ptr = convbuffer + i;
                  float *mono = pcm[i];
                  for (j = 0; j < bout; j++) {
                    int val = floor(mono[j] * 32767.f + .5f);
                    if (val > 32767) {
                      val = 32767;
                      clipflag = 1;
                    }
                    else if (val < -32768) {
                      val = -32768;
                      clipflag = 1;
                    }
                    *ptr = val;
                    ptr += vi.channels;
                  }
                }
                /** optional: print cerr for clipflag */

                // reallocate memory before writing
                const size_t sample_offset_delta = bout * vi.channels;
                const size_t required_pcm_buffer_size = (sample_offset + sample_offset_delta) * byte_per_sample;
                if (pcm_buffer_size < required_pcm_buffer_size)
                {
                  while (pcm_buffer_size < required_pcm_buffer_size)
                    pcm_buffer_size *= 2;
                  pcm_buffer = (char*)realloc(pcm_buffer, pcm_buffer_size);
                  ASSERT(pcm_buffer);
                }

                // write binary
                memcpy((int16_t*)pcm_buffer + sample_offset, convbuffer, byte_per_sample * sample_offset_delta);
                sample_offset += sample_offset_delta;

                vorbis_synthesis_read(&vd, bout); // tell libvorbis consumed sample count.
              }
            }
          }
          if (ogg_page_eos(&og)) eos = 1;
        }
      }
      if (!eos) {
        buffer = ogg_sync_buffer(&oy, kOGGDecodeBufferSize);
        bytes = fdd.Read((uint8_t*)buffer, kOGGDecodeBufferSize);
        if (bytes == 0) /* read all data */
        {
          eos = 1;
          break;
        } else ogg_sync_wrote(&oy, bytes);
      }
    }
  }

  // resize PCM data and give it to sound object
  pcm_buffer = (char*)realloc(pcm_buffer, sample_offset * byte_per_sample);
  sound().Set(byte_per_sample * 8, vi.channels, sample_offset / vi.channels, vi.rate, pcm_buffer);

  return pcm_buffer_size;
}

}