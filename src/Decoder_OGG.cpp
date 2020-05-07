#include "Decoder.h"
#include "Error.h"
#include "vorbis/vorbisfile.h"
#include <memory.h>

/** https://svn.xiph.org/trunk/vorbis/examples/decoder_example.c */

namespace rmixer
{

struct OGGDecodeContext
{
  rutil::FileData fdd;
  ogg_sync_state oy;
  ogg_stream_state os;
  ogg_page og;
  ogg_packet op;
  vorbis_info vi;
  vorbis_comment vc;
  vorbis_dsp_state vd;
  vorbis_block vb;
};

constexpr auto kOGGDecodeBufferSize = 8192u;
constexpr auto kOGGDefaultPCMBufferSize = 1024 * 1024 * 1u;  /* default allocating memory size for PCM decoding */

Decoder_OGG::Decoder_OGG()
  : pContext(0), buffer(0), bytes(0) {}

Decoder_OGG::~Decoder_OGG() { close(); }

bool Decoder_OGG::open(rutil::FileData &fd)
{
  close();
  pContext = new OGGDecodeContext();
  OGGDecodeContext &c = *(OGGDecodeContext*)pContext;

  ogg_sync_init(&c.oy);
  buffer = ogg_sync_buffer(&c.oy, kOGGDecodeBufferSize);
  bytes = fd.Read((uint8_t*)buffer, kOGGDecodeBufferSize);
  ogg_sync_wrote(&c.oy, bytes);
  if (ogg_sync_pageout(&c.oy, &c.og) != 1)
    return false;

  // ogg stream initization & RAII cleanup
  ogg_stream_init(&c.os, ogg_page_serialno(&c.og));

  vorbis_info_init(&c.vi);
  vorbis_comment_init(&c.vc);
  if (ogg_stream_pagein(&c.os, &c.og) < 0)
    return false;
  if (ogg_stream_packetout(&c.os, &c.op) != 1)
    return false;
  if (vorbis_synthesis_headerin(&c.vi, &c.vc, &c.op) < 0)
    return false;

  int i = 0;
  while (i < 2) {
    while (i < 2) {
      int result = ogg_sync_pageout(&c.oy, &c.og);
      if (result == 0) break; /* Need more data */
      if (result == 1) {
        ogg_stream_pagein(&c.os, &c.og);
        while (i < 2) {
          result = ogg_stream_packetout(&c.os, &c.op);
          if (result == 0) break;
          if (result < 0)
            return false;
          result = vorbis_synthesis_headerin(&c.vi, &c.vc, &c.op);
          if (result < 0)
            return false;
          i++;
        }
      }
    }
    buffer = ogg_sync_buffer(&c.oy, kOGGDecodeBufferSize);
    bytes = fd.Read((uint8_t*)buffer, kOGGDecodeBufferSize);
    if (bytes == 0 && i < 2) /* end of file before scanning all vorbis headers */
      return false;
    ogg_sync_wrote(&c.oy, bytes);
  }

  /* default bitsize is F32 */
  info_ = SoundInfo(2, 32, c.vi.channels, c.vi.rate);
  c.fdd = fd;
  return true;
}

bool Decoder_OGG::open(const char* p, size_t len)
{
  // TODO: something necessary to not release data with FileData
  rutil::FileData fd;
  fd.p = (uint8_t*)p;
  fd.len = len;
  bool r = open(fd);
  fd.p = 0;
  fd.len = 0;
  return r;
}

void Decoder_OGG::close()
{
  if (!pContext) return;
  buffer = 0;
  OGGDecodeContext &c = *(OGGDecodeContext*)pContext;
  c.fdd.p = 0;  // prevent to release filedata ptr
  vorbis_block_clear(&c.vb);
  vorbis_dsp_clear(&c.vd);
  ogg_stream_clear(&c.os);
  vorbis_comment_clear(&c.vc);
  vorbis_info_clear(&c.vi);
  ogg_sync_clear(&c.oy);
  delete &c;
  pContext = 0;
}

#define DISABLE_CLIPFLAG 0

uint32_t Decoder_OGG::read_internal(char **p, bool read_raw)
{
  if (!pContext)
    return 0;

  OGGDecodeContext &c = *(OGGDecodeContext*)pContext;

  const size_t byte_per_sample = info_.bitsize / 8;
  int eos = 0;
  int result = 0;
  int convframesize = kOGGDecodeBufferSize / byte_per_sample / c.vi.channels;
  char convbuffer[kOGGDecodeBufferSize];
  int sample_offset = 0;
  size_t pcm_buffer_size = kOGGDefaultPCMBufferSize;
  char* pcm_buffer = (char*)malloc(pcm_buffer_size);
  //sound().Set(16, vi.channels, sample_total_count, vi.rate);

  uint16_t u16;
  uint32_t u32;
  int16_t s16;
  int32_t s32;

  if (byte_per_sample % 2 == 1)
    return 0;

  if (vorbis_synthesis_init(&c.vd, &c.vi) == 0)
  {
    vorbis_block_init(&c.vd, &c.vb);

    while (!eos) {
      while (!eos) {
        result = ogg_sync_pageout(&c.oy, &c.og);
        if (result == 0) break; /* need more data */
        else if (result < 0) {
          // corrupt bitstream data
        }
        else {
          ogg_stream_pagein(&c.os, &c.og);
          while (1) {
            result = ogg_stream_packetout(&c.os, &c.op);
            if (result == 0) break; /* need more data */
            if (result < 0) {
              // missing or corrupt data (already checked above)
            }
            else {
              float **pcm;
              int frames;
              if (vorbis_synthesis(&c.vb, &c.op) == 0)
                vorbis_synthesis_blockin(&c.vd, &c.vb);

              while ((frames = vorbis_synthesis_pcmout(&c.vd, &pcm)) > 0) {
                int j;
                int clipflag = 0;
                int bout = (frames < convframesize ? frames : convframesize);
                for (int i = 0; i < c.vi.channels; i++) {
                  char *ptr = convbuffer + i * byte_per_sample;
                  float *mono = pcm[i];
                  for (j = 0; j < bout; j++) {

                    switch (info_.is_signed)
                    {
                    case 0:
                      switch (info_.bitsize)
                      {
                      case 8:
                        u16 = (uint16_t)floor((mono[j] + 1.0f) * 127.f + .5f);
                        if (u16 > 255) { clipflag = 1; u16 = 255; }
                        else { clipflag = 0; }
                        *(uint8_t*)ptr = (uint8_t)u16;
                        break;
                      case 16:
                        u32 = (uint32_t)floor((mono[j] + 1.0f) * 32767.f + .5f);
                        if (u32 > 65535) { clipflag = 1; u32 = 65535; }
                        *(uint16_t*)ptr = (uint16_t)u32;
                        break;
                      case 32:
                        *(uint32_t*)ptr = (uint32_t)floor((mono[j] + 1.0f) * 2147483647.f + .5f);
                        break;
                      default:
                        RMIXER_ASSERT(0);
                      }
                      break;
                    case 1:
                      switch (info_.bitsize)
                      {
                      case 8:
                        s16 = (int16_t)floor(mono[j] * 127.f + .5f);
                        if (s16 > 127) { clipflag = 1; s16 = 127; }
                        else { clipflag = 0; }
                        *(int8_t*)ptr = (int8_t)s16;
                        break;
                      case 16:
                        s32 = (int32_t)floor(mono[j] * 32767.f + .5f);
                        if (s32 > 32767) { clipflag = 1; s32 = 32767; }
                        else if (s32 < -32768) { clipflag = 1; s32 = -32768; }
                        *(int16_t*)ptr = (int16_t)s32;
                        break;
                      case 32:
                        *(int32_t*)ptr = (int32_t)floor(mono[j] * 2147483647.f + .5f);
                        break;
                      default:
                        RMIXER_ASSERT(0);
                      }
                      break;
                    case 2:
                      switch (info_.bitsize)
                      {
                      case 32:
                        *(float*)ptr = mono[j];
                        //*(float*)ptr = mono[j] > 1.0f ? 1.0f : (mono[j] < -1.0f ? -1.0f : mono[j]);
                        break;
                      case 64:
                        *(double*)ptr = mono[j];
                        break;
                      default:
                        RMIXER_ASSERT(0);
                      }
                      break;
                    default:
                      RMIXER_ASSERT(0);
                    }

                    ptr += c.vi.channels * byte_per_sample;
                  }
                }
                /** optional: print cerr for clipflag */

                // reallocate memory before writing
                const size_t sample_offset_delta = bout * c.vi.channels;
                const size_t required_pcm_buffer_size = (sample_offset + sample_offset_delta) * byte_per_sample;
                if (pcm_buffer_size < required_pcm_buffer_size)
                {
                  while (pcm_buffer_size < required_pcm_buffer_size)
                    pcm_buffer_size *= 2;
                  pcm_buffer = (char*)realloc(pcm_buffer, pcm_buffer_size);
                  RMIXER_ASSERT(pcm_buffer);
                }

                // write binary
                memcpy(pcm_buffer + sample_offset * byte_per_sample,
                  convbuffer,
                  sample_offset_delta * byte_per_sample);
                sample_offset += sample_offset_delta;

                vorbis_synthesis_read(&c.vd, bout); // tell libvorbis consumed sample count.
              }
            }
          }
          if (ogg_page_eos(&c.og)) eos = 1;
        }
      }
      if (!eos) {
        buffer = ogg_sync_buffer(&c.oy, kOGGDecodeBufferSize);
        bytes = c.fdd.Read((uint8_t*)buffer, kOGGDecodeBufferSize);
        if (bytes == 0) /* read all data */
        {
          eos = 1;
          break;
        } else ogg_sync_wrote(&c.oy, bytes);
      }
    }
  }

  // resize PCM data and give it to sound object
  pcm_buffer = (char*)realloc(pcm_buffer, sample_offset * byte_per_sample);
  *p = pcm_buffer;
  return sample_offset / c.vi.channels; /* frame_count */
}

}
