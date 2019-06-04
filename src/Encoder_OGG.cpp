#include "Encoder.h"
#include "vorbis/vorbisenc.h"
#include "rutil.h"
#include <time.h>

/** https://svn.xiph.org/trunk/vorbis/examples/encoder_example.c */

namespace rhythmus
{

constexpr int kDefaultOGGQualityLevel = 4;
constexpr int kOggStreamBufferSize = 1024;

class VorbisCleanupHelper {
public:
  ogg_stream_state os; /* take physical pages, weld into a logical
                       stream of packets */
  ogg_page         og; /* one Ogg bitstream page.  Vorbis packets are inside */
  ogg_packet       op; /* one raw packet of data for decode */

  vorbis_info      vi; /* struct that stores all the static vorbis bitstream
                       settings */
  vorbis_comment   vc; /* struct that stores all the user comments */

  vorbis_dsp_state vd; /* central working state for the packet->PCM decoder */
  vorbis_block     vb; /* local working space for packet->PCM decode */

  ~VorbisCleanupHelper()
  {
    ogg_stream_clear(&os);
    vorbis_block_clear(&vb);
    vorbis_dsp_clear(&vd);
    vorbis_comment_clear(&vc);
    vorbis_info_clear(&vi);
  }
};

Encoder_OGG::Encoder_OGG(const Sound& sound)
  : Encoder(sound), quality_level(kDefaultOGGQualityLevel)
{
  initbufferread();
}

Encoder_OGG::Encoder_OGG(const SoundMixer& mixer)
  : Encoder(mixer), quality_level(kDefaultOGGQualityLevel)
{
  initbufferread();
}

bool Encoder_OGG::Write(const std::string& path)
{
  FILE *fp = rutil::fopen_utf8(path.c_str(), "wb");
  if (!fp)
    return false;

  VorbisCleanupHelper vorbis;

  ogg_stream_state &os = vorbis.os;
  ogg_page         &og = vorbis.og;
  ogg_packet       &op = vorbis.op;
  vorbis_info      &vi = vorbis.vi;
  vorbis_comment   &vc = vorbis.vc;
  vorbis_dsp_state &vd = vorbis.vd;
  vorbis_block     &vb = vorbis.vb;

  int eos = 0, ret = 0;

  vorbis_info_init(&vi);
  ret = vorbis_encode_init_vbr(&vi, info_.channels, info_.rate, quality_level);
  /** in case of ABR,
  ret = vorbis_encode_init(&vi, info_.channels, info_.rate, -1, 128000, -1);
  */

  if (ret) return false;

  /* metadata goes here */
  vorbis_comment_init(&vc);
  vorbis_comment_add_tag(&vc, "ENCODER", "Rhythmus-Encoder");
  {
    std::string metavalue;
    if (GetMetadata("TITLE", metavalue))
      vorbis_comment_add_tag(&vc, "TITLE", metavalue.c_str());
    if (GetMetadata("ARTIST", metavalue))
      vorbis_comment_add_tag(&vc, "ARTIST", metavalue.c_str());
  }

  vorbis_analysis_init(&vd, &vi);
  vorbis_block_init(&vd, &vb);

  srand(time(0));
  ogg_stream_init(&os, rand());

  /* write header I/O */
  {
    ogg_packet header;
    ogg_packet header_comm;
    ogg_packet header_code;

    vorbis_analysis_headerout(&vd, &vc, &header, &header_comm, &header_code);
    ogg_stream_packetin(&os, &header); /* automatically placed in its own
                                       page */
    ogg_stream_packetin(&os, &header_comm);
    ogg_stream_packetin(&os, &header_code);

    /* This ensures the actual
     * audio data will start on a new page, as per spec
     */
    while (!eos) {
      int result = ogg_stream_flush(&os, &og);
      if (result == 0)break;
      fwrite(og.header, 1, og.header_len, fp);
      fwrite(og.body, 1, og.body_len, fp);
    }
  }

  /* start writing samples */
  char *readbuffer = (char*)malloc(kOggStreamBufferSize * 4);
  while (!eos) {
    long i;
    long bytes = bufferread(readbuffer, kOggStreamBufferSize * 4);

    if (bytes == 0) {
      /* end of file.  this can be done implicitly in the mainline,
      but it's easier to see here in non-clever fashion.
      Tell the library we're at end of stream so that it can handle
      the last frame and mark end of stream in the output properly */
      vorbis_analysis_wrote(&vd, 0);
    }
    else {
      /* data to encode */

      /* expose the buffer to submit data */
      float **buffer = vorbis_analysis_buffer(&vd, kOggStreamBufferSize);

      /* uninterleave samples */
      for (i = 0; i<bytes / 4; i++) {
        buffer[0][i] = ((readbuffer[i * 4 + 1] << 8) |
          (0x00ff & (int)readbuffer[i * 4])) / 32768.f;
        buffer[1][i] = ((readbuffer[i * 4 + 3] << 8) |
          (0x00ff & (int)readbuffer[i * 4 + 2])) / 32768.f;
      }

      /* tell the library how much we actually submitted */
      vorbis_analysis_wrote(&vd, i);
    }

    /* vorbis does some data preanalysis, then divides up blocks for
    more involved (potentially parallel) processing.  Get a single
    block for encoding now */
    while (vorbis_analysis_blockout(&vd, &vb) == 1) {

      /* analysis, assume we want to use bitrate management */
      vorbis_analysis(&vb, NULL);
      vorbis_bitrate_addblock(&vb);

      while (vorbis_bitrate_flushpacket(&vd, &op)) {

        /* weld the packet into the bitstream */
        ogg_stream_packetin(&os, &op);

        /* write out pages (if any) */
        while (!eos) {
          int result = ogg_stream_pageout(&os, &og);
          if (result == 0) break;
          fwrite(og.header, 1, og.header_len, fp);
          fwrite(og.body, 1, og.body_len, fp);

          /* this could be set above, but for illustrative purposes, I do
          it here (to show that vorbis does know where the stream ends) */

          if (ogg_page_eos(&og))eos = 1;
        }
      }
    }
  }

  /* end */
  free(readbuffer);
  fclose(fp);
  return true;
}

void Encoder_OGG::initbufferread()
{
  current_buffer_index = 0;
  current_buffer_offset = 0;
}

long Encoder_OGG::bufferread(char* pOut, size_t size)
{
  if (current_buffer_index == buffers_.size())
    return 0;

  long readsize = 0;
  while (size > 0 && current_buffer_index < buffers_.size())
  {
    long cpysize = buffers_[current_buffer_index].s - current_buffer_offset;
    if (cpysize > size) cpysize = size;
    memcpy(pOut, buffers_[current_buffer_index].p, cpysize);
    pOut += cpysize;
    current_buffer_offset += cpysize;
    size -= cpysize;
    readsize += cpysize;

    if (current_buffer_offset >= buffers_[current_buffer_index].s)
    {
      current_buffer_index++;
      current_buffer_offset = 0;
    }
  }

  return readsize;
}

}