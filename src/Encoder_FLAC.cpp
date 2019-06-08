#include "Encoder.h"
#include "FLAC/stream_encoder.h"
#include "FLAC/metadata.h"

namespace rhythmus
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

  /* init */
  encoder = FLAC__stream_encoder_new();

  FLAC__stream_encoder_set_verify(encoder, true);
  FLAC__stream_encoder_set_compression_level(encoder, 5);
  FLAC__stream_encoder_set_channels(encoder, info_.channels);
  FLAC__stream_encoder_set_bits_per_sample(encoder, info_.bitsize);
  FLAC__stream_encoder_set_sample_rate(encoder, info_.rate);
  FLAC__stream_encoder_set_total_samples_estimate(encoder, total_buffer_size_ * 8 / info_.bitsize);

  init_status = FLAC__stream_encoder_init_file(encoder, path.c_str(), /* progress_callback */0, /*client_data=*/NULL);
  if (init_status != FLAC__STREAM_ENCODER_INIT_STATUS_OK)
  {
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

  /* encoding */
  for (auto &ii : buffers_)
  {
    size_t samples = ii.s * 8 / info_.bitsize;
    FLAC__stream_encoder_process_interleaved(encoder, (int32_t*)ii.s, samples);
  }
  FLAC__stream_encoder_finish(encoder);

  /* cleanup */
  FLAC__metadata_object_delete(metadata[0]);
  FLAC__metadata_object_delete(metadata[1]);
  FLAC__stream_encoder_delete(encoder);

  return true;
}

}