#include "Decoder.h"

/** https://svn.xiph.org/trunk/vorbis/examples/decoder_example.c */

namespace rhythmus
{

  constexpr auto kOGGDecodeBufferSize = 4096u;

Decoder_OGG::Decoder_OGG(Sound &s) : Decoder(s) {}

bool Decoder_OGG::open(rutil::FileData &fd)
{
  fdd = fd;
  ogg_sync_init(&oy);
  buffer = ogg_sync_buffer(&oy, kOGGDecodeBufferSize);
  bytes = fd.Read((uint8_t*)buffer, kOGGDecodeBufferSize);
  ogg_sync_wrote(&oy, bytes);
  if (ogg_sync_pageout(&oy, &og) != 1)
    return false;

  ogg_stream_init(&os, ogg_page_serialno(&og));
  vorbis_info_init(&vi);
  vorbis_comment_init(&vc);
  if (ogg_stream_pagein(&os, &og) != 1)
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
  int eos = 0;
  int result = 0;
  int convsamplesize = kOGGDecodeBufferSize / vi.channels;
  ogg_int16_t convbuffer[kOGGDecodeBufferSize];
  sound().Set(16, vi.channels, vi.bitrate_window, vi.rate);

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
          while (1) {
            result = ogg_stream_packetout(&os, &op);
            if (result == 0) break;
            if (result < 0) {
              // missing or corrupt data (already checked above)
            }
            else {
              float **pcm;
              int samples;
              if (vorbis_synthesis(&vb, &op) == 0)
                vorbis_synthesis_blockin(&vd, &vb);

              while ((samples = vorbis_synthesis_pcmout(&vd, &pcm)) > 0) {
                int j;
                int clipflag = 0;
                int bout = (samples < convsamplesize ? samples : convsamplesize);
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

                // write binary

                vorbis_synthesis_read(&vd, bout);
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
          return true;
        ogg_sync_wrote(&oy, bytes);
      }
    }
  }

  return false;
}

}