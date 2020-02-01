#include "Encoder.h"
#include "Error.h"

#define FLAC__NO_DLL
#include "FLAC/stream_encoder.h"
#include "FLAC/metadata.h"
#include <iostream>

#ifndef DO_NOT_USE_RUTIL
#include "rparser.h"  /* rutil module */
#endif

#define FLAC_READSIZE 2048

namespace rmixer
{

Encoder_FLAC::Encoder_FLAC(const PCMBuffer& sound) : Encoder(sound)
{
}

bool Encoder_FLAC::Write(const std::string& path)
{
  FLAC__StreamEncoder *encoder = 0;
  FLAC__StreamEncoderInitStatus init_status;
  FLAC__StreamMetadata *metadata[2];
  FLAC__StreamMetadata_VorbisComment_Entry entry;
  bool encoding_res = true;

  /* init */
  encoder = FLAC__stream_encoder_new();

  //FLAC__stream_encoder_set_verify(encoder, true);
  FLAC__stream_encoder_set_compression_level(encoder, 5);
  FLAC__stream_encoder_set_channels(encoder, info_.channels);
  FLAC__stream_encoder_set_bits_per_sample(encoder, info_.bitsize);
  FLAC__stream_encoder_set_sample_rate(encoder, info_.rate);
  FLAC__stream_encoder_set_total_samples_estimate(encoder, total_buffer_size_ * 8 / info_.bitsize);

  /* init stream */
  FILE *f =
#ifndef DO_NOT_USE_RUTIL
    rutil::fopen_utf8(path, "wb");
#else
    fopen(path.c_str(), "wb");
#endif
  if (f)
    FLAC__stream_encoder_init_FILE(encoder, f, 0, 0);
  else {
    std::cerr << "Error : Cannot open FLAC output file path." << std::endl;
    return false;
  }

  /* metadata */
  {
    metadata[0] = FLAC__metadata_object_new(FLAC__METADATA_TYPE_VORBIS_COMMENT);
    FLAC__metadata_object_vorbiscomment_entry_from_name_value_pair(&entry, "TITLE", "Some title");
    FLAC__metadata_object_vorbiscomment_append_comment(metadata[0], entry, /*copy=*/false);
    FLAC__metadata_object_vorbiscomment_entry_from_name_value_pair(&entry, "ARTIST", "Some Artist");
    FLAC__metadata_object_vorbiscomment_append_comment(metadata[0], entry, /*copy=*/false);

    metadata[1] = FLAC__metadata_object_new(FLAC__METADATA_TYPE_PADDING);
    metadata[1]->length = 1234;

    FLAC__stream_encoder_set_metadata(encoder, metadata, 2);
  }

  /* prepare buffer for encoding ... */
  const size_t bps = info_.bitsize / 8;
  int32_t* buf = 0;
  size_t bufsize = 0;
  for (auto &ii : buffers_)
    if (bufsize < ii.s) bufsize = ii.s;
  buf = (int32_t*)malloc(bufsize * sizeof(int32_t) / bps);

  /* encoding */
  for (auto &ii : buffers_)
  {
    size_t bps = info_.bitsize / 8;
    size_t samples = ii.s / bps;
    int32_t* bufptr = buf;
    // upcast sample data if necessary
    switch (info_.bitsize)
    {
    case 32:
      bufptr = (int32_t*)ii.s;
      break;
    case 24:
      for (auto i = 0; i < samples; ++i)
        bufptr[i] = (int32_t)(*(int32_t*)(&ii.p[i * 3]) & 0x00ffffff);
      break;
    case 16:
      for (auto i = 0; i < samples; ++i)
        bufptr[i] = (int32_t)*(int16_t*)(&ii.p[i * 2]);
      break;
    case 8:
      for (auto i = 0; i < samples; ++i)
        bufptr[i] = (int32_t)ii.p[i];
      break;
    default:
      ASSERT(0);
    }
    encoding_res &= FLAC__stream_encoder_process_interleaved(encoder, bufptr, samples / info_.channels);
  }
  encoding_res &= FLAC__stream_encoder_finish(encoder);

  if (!encoding_res)
    std::cerr << FLAC__StreamEncoderStateString[FLAC__stream_encoder_get_state(encoder)] << std::endl;

  /* cleanup */
  if (buf) free(buf);
  FLAC__metadata_object_delete(metadata[0]);
  FLAC__metadata_object_delete(metadata[1]);
  FLAC__stream_encoder_delete(encoder);

  return encoding_res;
}

}
