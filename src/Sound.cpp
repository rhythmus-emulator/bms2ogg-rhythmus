#include "Sound.h"
#include "Error.h"

namespace rhythmus
{

bool operator==(const SoundInfo& a, const SoundInfo& b)
{
  return a.bitsize == b.bitsize &&
         a.rate == b.rate &&
         a.channels == b.channels;
}

Sound::Sound()
  : buffer_(0), buffer_size_(0) /* buffer size in bit */
{
  memset(&info_, 0, sizeof(SoundInfo));
}

Sound::~Sound()
{
  Clear();
}

void Sound::Set(uint16_t bitsize, uint8_t channels, size_t framecount, uint32_t rate, void* p)
{
  Clear();
  info_.bitsize = bitsize;
  info_.channels = channels;
  info_.rate = rate;
  buffer_size_ = bitsize * channels * framecount;
  if (!p)
    buffer_ = (int8_t*)malloc(buffer_size_ / 8);
  else
    buffer_ = (int8_t*)p;
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

const size_t Sound::buffer_byte_size() const
{
  return buffer_size_ / 8;
}


SoundMixer::SoundMixer(size_t chunk_size)
  : chunk_size_(chunk_size), total_size_(0), sample_count_in_chunk_(0)
{
  ASSERT(chunk_size % 64 == 0);
  SetInfo({ 32, 2, 44100 });
}

SoundMixer::~SoundMixer()
{
  Clear();
}

void SoundMixer::SetInfo(const SoundInfo& info)
{
  info_ = info;
  sample_count_in_chunk_ = chunk_size_ / info.channels / (info.bitsize / 8);
}

void SoundMixer::Mix(Sound& s, uint32_t ms)
{
  ASSERT(sample_count_in_chunk_ > 0);
  ASSERT(s.get_info() == info_);

  uint32_t totalwritesize = s.buffer_byte_size();
  uint32_t startoffset = Time2Byteoffset(ms);
  uint32_t endoffset = startoffset + totalwritesize;
  PrepareByteoffset(endoffset);
  size_t chunk_idx = startoffset / chunk_size_;
  size_t chunk_endpos = chunk_size_ * (chunk_idx + 1);
  size_t writtensize = 0;
  while (writtensize < s.buffer_byte_size())
  {
    const size_t dest_buf_offset = (startoffset + writtensize) % chunk_size_;
    const size_t writingsize = totalwritesize > (chunk_size_ - dest_buf_offset) ? (chunk_size_ - dest_buf_offset) : totalwritesize;
    MixToChunk(s.ptr() + writtensize, writingsize, buffers_[chunk_idx] + dest_buf_offset);
    writtensize += writingsize;
    totalwritesize -= writingsize;
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

size_t SoundMixer::get_total_size() const
{
  return total_size_;
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
  return sample_idx * info_.bitsize / 8 * info_.channels;
}

void SoundMixer::PrepareByteoffset(uint32_t offset)
{
  if (total_size_ > offset) return;
  else
  {
    total_size_ = offset;
    size_t chunk_necessary_count = offset / chunk_size_;
    if (chunk_size_ % offset > 0)
      chunk_necessary_count++;
    while (buffers_.size() < chunk_necessary_count)
    {
      buffers_.push_back((int8_t*)malloc(chunk_size_));
      memset(buffers_.back(), 0, chunk_size_);
    }
  }
}

template <typename T>
void MixSampleWithClipping(T* dest, T* source)
{
  // for fast processing
  if (*dest == 0)
    *dest = *source;
  else if (*dest > 0 && *dest > std::numeric_limits<T>::max() - *source)
    *dest = std::numeric_limits<T>::max();
  else if (*dest < 0 && *dest < std::numeric_limits<T>::min() - *source)
      *dest = std::numeric_limits<T>::min();
  else
    *dest += *source;
}

void SoundMixer::MixToChunk(int8_t *source, size_t source_size, int8_t *dest)
{
  size_t i = 0;
  while (i < source_size)
  {
    switch (info_.bitsize)
    {
    case 8:
      // clipping sound if necessary
      MixSampleWithClipping(dest, source);
      dest++;
      source++;
      i++;
      break;
    case 16:
      MixSampleWithClipping(reinterpret_cast<int16_t*>(dest), reinterpret_cast<int16_t*>(source));
      dest += 2;
      source += 2;
      i += 2;
      break;
    case 32:
      MixSampleWithClipping(reinterpret_cast<int32_t*>(dest), reinterpret_cast<int32_t*>(source));
      dest += 4;
      source += 4;
      i += 4;
      break;
    default:
      // 4Byte PCM won't be supported
      ASSERT(0);
    }
  }
}

}