#ifndef RENCODER_H
#define RENCODER_H

#include "Song.h"

namespace rhythmus
{

class Encoder
{
public:
  Encoder();
  ~Encoder();
private:
  rparser::Song song;
};

}

#endif