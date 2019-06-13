#include "Decoder.h"
#include <iostream>

#define FLAC__NO_DLL
#include "FLAC/stream_decoder.h"

namespace rhythmus
{

/** internal stream decoder functions */
FLAC__StreamDecoderReadStatus read_cb(const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes, void *client_data)
{
  rutil::FileData &fd = ((Decoder_FLAC*)client_data)->get_fd();
  if (*bytes > 0) {
    *bytes = fd.Read(buffer, sizeof(FLAC__byte) * *bytes);
    if (*bytes == 0)
      return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
    else
      return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
  }
  else
    return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
}

FLAC__StreamDecoderSeekStatus seek_cb(const FLAC__StreamDecoder *decoder, FLAC__uint64 absolute_byte_offset, void *client_data)
{
  rutil::FileData &fd = ((Decoder_FLAC*)client_data)->get_fd();
  fd.SeekSet(absolute_byte_offset);
  return FLAC__STREAM_DECODER_SEEK_STATUS_OK;
}

FLAC__StreamDecoderTellStatus tell_cb(const FLAC__StreamDecoder *decoder, FLAC__uint64 *absolute_byte_offset, void *client_data)
{
  rutil::FileData &fd = ((Decoder_FLAC*)client_data)->get_fd();
  off_t pos;
  
  if ((pos = fd.GetPos()) < 0)
    return FLAC__STREAM_DECODER_TELL_STATUS_ERROR;
  else {
    *absolute_byte_offset = (FLAC__uint64)pos;
    return FLAC__STREAM_DECODER_TELL_STATUS_OK;
  }
}

FLAC__StreamDecoderLengthStatus length_cb(const FLAC__StreamDecoder *decoder, FLAC__uint64 *stream_length, void *client_data)
{
  rutil::FileData &fd = ((Decoder_FLAC*)client_data)->get_fd();
  *stream_length = (FLAC__uint64)fd.GetFileSize();
  return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
}

FLAC__bool eof_cb(const FLAC__StreamDecoder *decoder, void *client_data)
{
  rutil::FileData &fd = ((Decoder_FLAC*)client_data)->get_fd();
  return fd.IsEOF();
}

FLAC__StreamDecoderWriteStatus write_cb(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data)
{
  // get decoded PCM here.
  Decoder_FLAC* f = ((Decoder_FLAC*)client_data);
  const auto real_bps = frame->header.bits_per_sample;
  const auto bps = f->info().bitsize;
  const auto byps = bps / 8;
  uint32_t offset = 0;
  size_t channelcnt = f->info().channels;
  const size_t shift_byte_size = 4 - byps;

  // make interleaved PCM data here
  for (unsigned ch = 0; ch < channelcnt; ch++) {
    const int32_t* p = buffer[ch];
    offset = f->buffer_pos_ + byps * ch;
    for (unsigned i = 0; i < frame->header.blocksize; i++) {
      //memcpy(f->buffer_ + offset, (int8_t*)(p + i) + shift_byte_size, byps);  // Big Endian (XXX: need to reverse byte order?)
      memcpy(f->buffer_ + offset, (int8_t*)(p + i), byps);    // Little Endian
      offset += byps * channelcnt;
    }
  }

  f->buffer_pos_ += frame->header.blocksize * channelcnt * byps;
  return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

void meta_cb(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data)
{
  // only fetch if streaminfo
  if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO) {
    /* save for later */
    uint32_t sample_rate = metadata->data.stream_info.sample_rate;
    uint32_t channels = metadata->data.stream_info.channels;
    uint32_t bps = metadata->data.stream_info.bits_per_sample;
    uint32_t total_samples = metadata->data.stream_info.total_samples * channels;

    /* no 24bit */
    if (bps == 24)
      bps = 32;

    Decoder_FLAC* f = ((Decoder_FLAC*)client_data);
    f->info().bitsize = bps;
    f->info().rate = sample_rate;
    f->info().channels = channels;
    f->total_samples_ = total_samples;

    // allocate buffer here
    const FLAC__uint32 total_size = (FLAC__uint32)(total_samples * (bps / 8));
    f->buffer_ = (uint8_t*)malloc(total_size);
  }
}

void error_cb(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data)
{
  std::cerr << "FLAC decoding error: " << FLAC__StreamDecoderErrorStatusString[status] << std::endl;
}
/** internal stream decoder functions end */

Decoder_FLAC::Decoder_FLAC(Sound &s) : Decoder(s), total_samples_(0), buffer_(0), pContext_(0)
{
}

bool Decoder_FLAC::open(rutil::FileData &fd)
{
  close();
  FLAC__StreamDecoder *decoder;
  FLAC__StreamDecoderInitStatus init_status;

  this->fd_ = fd;

  pContext_ = decoder = FLAC__stream_decoder_new();
  init_status = FLAC__stream_decoder_init_stream(decoder,
    read_cb, seek_cb, tell_cb, length_cb, eof_cb,
    write_cb, meta_cb, error_cb,
    this);

  if (init_status != FLAC__STREAM_DECODER_INIT_STATUS_OK)
    return false;

  return true;
}

void Decoder_FLAC::close()
{
  if (pContext_)
  {
    FLAC__stream_decoder_delete((FLAC__StreamDecoder*)pContext_);
    pContext_ = 0;
  }
  if (buffer_)
  {
    free(buffer_);
    buffer_ = 0;
  }
}

uint32_t Decoder_FLAC::read()
{
  if (!pContext_)
    return 0;
  buffer_pos_ = 0;
  bool r = FLAC__stream_decoder_process_until_end_of_stream((FLAC__StreamDecoder*)pContext_);

  if (r)
  {
    sound().Set(info_.bitsize, info_.channels, total_samples_ / info_.channels, info_.rate, buffer_);
    buffer_ = 0;
    return total_samples_;
  }
  else return 0;
}

rutil::FileData& Decoder_FLAC::get_fd()
{
  return fd_;
}

SoundInfo& Decoder_FLAC::info()
{
  return info_;
}


}