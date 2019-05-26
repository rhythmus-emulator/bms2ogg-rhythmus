#include "Sound.h"

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

void Sound::Clear()
{
  if (buffer_)
  {
    free(buffer_);
    buffer_ = 0;
    buffer_size_ = 0;
  }
}

}