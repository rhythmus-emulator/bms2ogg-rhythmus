#include "Sound.h"
#include "Error.h"

namespace rhythmus
{

Sound::Sound()
  : buffer_(0), buffer_size_(0)
{
  memset(&info_, 0, sizeof(SoundInfo));
}

Sound::~Sound()
{
  Clear();
}

void Sound::Set(uint16_t bitsize, uint8_t channels, size_t framecount, uint32_t rate)
{
  Clear();
  info_.bitsize = bitsize;
  info_.channels = channels;
  info_.rate = rate;
  buffer_size_ = bitsize * channels * framecount;
  buffer_ = (int8_t*)malloc(buffer_size_);
}

size_t Sound::GetFrameCount() const
{
  return buffer_size_ / info_.bitsize / info_.channels;
}

int8_t* Sound::ptr()
{
  return buffer_;
}

const int8_t* Sound::ptr() const
{
  return buffer_;
}

void Sound::Clear()
{
  if (buffer_)
  {
    free(buffer_);
    buffer_ = 0;
    buffer_size_ = 0;
  }
}

const SoundInfo& Sound::get_info() const
{
  return info_;
}

const size_t Sound::buffer_size() const
{
  return buffer_size_;
}


SoundMixer::SoundMixer(size_t chunk_size)
  : chunk_size_(chunk_size), sample_count_in_chunk_(0)
{
  ASSERT(chunk_size % 64 == 0);
  SetInfo(32, 2, 44100);
}

SoundMixer::~SoundMixer()
{
  Clear();
}

void SoundMixer::SetInfo(uint16_t bitsize, uint8_t channels, uint32_t rate)
{
  info_.bitsize = bitsize;
  info_.channels = channels;
  info_.rate = rate;
  sample_count_in_chunk_ = chunk_size_ / channels / bitsize;
}

void SoundMixer::Mix(Sound& s, uint32_t ms)
{
  ASSERT(sample_count_in_chunk_ > 0);
  ASSERT(memcmp(&s.get_info(), &info_, sizeof(SoundInfo)) == 0);

  uint32_t byteoffset = Time2Byteoffset(ms);
  uint32_t endoffset = byteoffset + s.buffer_size();
  PrepareByteoffset(endoffset);
  size_t chunk_idx = byteoffset / chunk_size_;
  size_t chunk_endpos = chunk_size_ * (chunk_idx + 1);
  size_t writtensize = 0;
  while (writtensize < s.buffer_size())
  {
    const size_t writingsize = chunk_endpos - byteoffset;
    MixToChunk(s.ptr() + writtensize, writingsize, buffers_[chunk_idx]);
    writtensize += writingsize;
    byteoffset = chunk_endpos;
    chunk_endpos += chunk_size_;
    chunk_idx++;
  }
}

const SoundInfo& SoundMixer::get_info() const
{
  return info_;
}

size_t SoundMixer::get_chunk_size() const
{
  return chunk_size_;
}

size_t SoundMixer::get_chunk_count() const
{
  return buffers_.size();
}

int8_t* SoundMixer::get_chunk(size_t idx)
{
  ASSERT(idx < buffers_.size());
  return buffers_[idx];
}

const int8_t* SoundMixer::get_chunk(size_t idx) const
{
  ASSERT(idx < buffers_.size());
  return buffers_[idx];
}

void SoundMixer::Clear()
{
  for (auto *p : buffers_)
    free(p);
  buffers_.clear();
}

uint32_t SoundMixer::Time2Byteoffset(double ms)
{
  // Time to sample
  uint32_t sample_idx = static_cast<uint32_t>(info_.rate / 1000.0 * ms);
  // Sample to byte
  return sample_idx * info_.bitsize * info_.channels;
}

void SoundMixer::PrepareByteoffset(uint32_t offset)
{
  size_t chunk_necessary_count = chunk_size_ / offset;
  if (chunk_size_ % offset > 0)
    chunk_necessary_count++;
  while (buffers_.size() < chunk_necessary_count)
  {
    buffers_.push_back((int8_t*)malloc(chunk_size_));
  }
}

void SoundMixer::MixToChunk(int8_t *source, size_t source_size, int8_t *dest)
{
  size_t byte_offset = chunk_size_ - source_size;
  dest += byte_offset / 8;
  while (byte_offset < chunk_size_)
  {
    switch (info_.bitsize)
    {
    case 8:
      *static_cast<int8_t*>(dest) += *static_cast<int8_t*>(source);
      dest++;
      source++;
      break;
    case 16:
      *reinterpret_cast<int16_t*>(dest) += *reinterpret_cast<int16_t*>(source);
      dest += 2;
      source += 2;
      break;
    case 32:
      *reinterpret_cast<int32_t*>(dest) += *reinterpret_cast<int32_t*>(source);
      dest += 4;
      source += 4;
      break;
    default:
      // 4Byte PCM won't be supported
      ASSERT(0);
    }
    byte_offset += info_.bitsize;
  }
}

}