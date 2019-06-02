#ifndef RENCODER_DECODER_H
#define RENCODER_DECODER_H

#include <stdint.h>
#include "rutil.h"
#include "Sound.h"
#include "vorbis/vorbisfile.h"

namespace rhythmus
{

class Decoder
{
public:
  Decoder(Sound &s);
  ~Decoder();
  virtual bool open(rutil::FileData &fd) = 0;
  virtual void close();
  virtual uint32_t read() = 0;
  Sound& sound();
private:
  Sound *s_;
};

class Decoder_WAV : public Decoder
{
public:
  Decoder_WAV(Sound &s);
  virtual bool open(rutil::FileData &fd);
  virtual void close();
  virtual uint32_t read();
  uint32_t readAsS32(); // depreciated
private:
  void* pWav_;
};

class Decoder_OGG : public Decoder
{
public:
  Decoder_OGG(Sound &s);
  virtual bool open(rutil::FileData &fd);
  virtual void close();
  virtual uint32_t read();
private:
  rutil::FileData fdd;
  ogg_sync_state oy;
  ogg_stream_state os;
  ogg_page og;
  ogg_packet op;
  vorbis_info vi;
  vorbis_comment vc;
  vorbis_dsp_state vd;
  vorbis_block vb;
  char *buffer;
  int bytes;
};

}

#endif