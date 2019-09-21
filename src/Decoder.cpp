#include "Decoder.h"

namespace rmixer
{

Decoder::Decoder() {}

Decoder::~Decoder() { close(); }

void Decoder::close() { }

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