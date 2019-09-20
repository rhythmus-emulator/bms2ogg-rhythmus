#include "Decoder.h"

namespace rmixer
{

Decoder::Decoder(Sound &s) : s_(&s) {}

Decoder::~Decoder() { close(); }

Sound& Decoder::sound() { return *s_; }

void Decoder::close() { }


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