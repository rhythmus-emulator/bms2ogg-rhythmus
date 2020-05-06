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

Encoder_FLAC::Encoder_FLAC(const Sound& sound) : Encoder(sound)
{
}

bool Encoder_FLAC::Write(const std::string& path)
{
  FLAC__StreamEncoder *encoder = 0;
  FLAC__StreamEncoderInitStatus init_status;
  FLAC__StreamMetadata *metadata[2];
  FLAC__StreamMetadata_VorbisComment_Entry entry;
  FLAC__bool encoding_res = true;

  /* init */
  encoder = FLAC__stream_encoder_new();

  //FLAC__stream_encoder_set_verify(encoder, true);
  FLAC__stream_encoder_set_compression_level(encoder, 5);
  FLAC__stream_encoder_set_channels(encoder, info_.channels);
  FLAC__stream_encoder_set_bits_per_sample(encoder, 24 /* wanna 24bit FLAC always ...? */);
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
  {
    if (FLAC__stream_encoder_init_FILE(encoder, f, 0, 0) != FLAC__STREAM_ENCODER_OK)
    {
      // TODO: remove cerr and replace it with encoder error code.
      std::cerr << "Error : FLAC initialization failed." << std::endl;
      return false;
    }
  }
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
  size_t bytesize = 0;
  for (auto &ii : buffers_)
    if (bytesize < ii.s) bytesize = ii.s;
  buf = (int32_t*)malloc(bytesize / bps * sizeof(int32_t));

  /* encoding */
  for (auto &ii : buffers_)
  {
    size_t bps = info_.bitsize / 8;
    size_t samples = ii.s / bps;
    int32_t* bufptr = buf;
    // upcast sample data if necessary
    if (info_.is_signed >= 1)
    {
      switch (info_.bitsize)
      {
      case 64:
        RMIXER_ASSERT(info_.is_signed == 2);
        for (size_t i = 0; i < samples; ++i)
          bufptr[i] = (int32_t)(*(double*)(ii.p + i * 8) * 0x7fffffff);
      case 32:
        /* @warn  FLAC only supports integer, need to convert if it's float */
        if (info_.is_signed == 2)
          for (size_t i = 0; i < samples; ++i)
            bufptr[i] = (int32_t)(*(float*)(ii.p + i * 4) * 0x7fffffff);
        else
          bufptr = (int32_t*)ii.s;
        break;
      case 24:
        for (size_t i = 0; i < samples; ++i)
          bufptr[i] = (*(int32_t*)(&ii.p[i * 3]) & 0x00ffffff) << 8;
        break;
      case 16:
        for (size_t i = 0; i < samples; ++i)
          bufptr[i] = *(int16_t*)(&ii.p[i * 2]) << 16;
        break;
      case 8:
        for (size_t i = 0; i < samples; ++i)
          bufptr[i] = ii.p[i] << 24;
        break;
      default:
        RMIXER_ASSERT(0);
      }
    }
    else
    {
      switch (info_.bitsize)
      {
      case 32:
        for (size_t i = 0; i < samples; ++i)
          bufptr[i] = *(uint32_t*)(ii.p + i * 4) - 0x7fffffff;
        break;
      case 24:
        for (size_t i = 0; i < samples; ++i)
          bufptr[i] = ((*(uint32_t*)(&ii.p[i * 3]) & 0x00ffffff) << 8) - 0x7fffffff;
        break;
      case 16:
        for (size_t i = 0; i < samples; ++i)
          bufptr[i] = (*(uint16_t*)(&ii.p[i * 2]) - 0x7fff) << 16;
        break;
      case 8:
        for (size_t i = 0; i < samples; ++i)
          bufptr[i] = ((uint8_t)ii.p[i] - 0x7f) << 24;
        break;
      default:
        RMIXER_ASSERT(0);
      }
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
