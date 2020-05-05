#include "Decoder.h"

namespace rmixer
{

Decoder::Decoder() {}

Decoder::~Decoder() {}

uint32_t Decoder::readWithFormat(char** p, const SoundInfo& info)
{
  SoundInfo info_prev = info_;
  info_ = info;
  uint32_t r = read_internal(p, true);
  if (r == 0)
  {
    // if failed to read with given sound format, rollback previous soundinfo.
    info_ = info_prev;
  }
  return r;
}

uint32_t Decoder::read(char** p)
{
  return read_internal(p, false);
}

const SoundInfo& Decoder::get_info() { return info_; }


#if 0

size_t Decoder::read(size_t bytesToRead)
{

}

int Decoder::open()
{

}

void Decoder::close()
{

}

uint32_t Decoder::read()
{

}

int Decoder::rewind()
{

}

int Decoder::seek(uint32_t ms)
{

}
#endif

}