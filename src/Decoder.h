#ifndef RMIXER_DECODER_H
#define RMIXER_DECODER_H

#include <stdint.h>
#include "rparser.h" /* due to rutil module */
#include "Sound.h"

namespace rmixer
{

class Decoder
{
public:
  Decoder();
  virtual ~Decoder();
  virtual bool open(rutil::FileData &fd) = 0;
  virtual bool open(const char* p, size_t len) = 0;
  virtual void close() = 0;

  /**
   * @param create buffer to get
   * @return frame count
   */
  virtual uint32_t read(char** p) = 0;
  const SoundInfo& get_info();

protected:
  SoundInfo info_;
};

class Decoder_WAV : public Decoder
{
public:
  Decoder_WAV();
  virtual ~Decoder_WAV();
  virtual bool open(rutil::FileData &fd);
  virtual bool open(const char* p, size_t len);
  virtual void close();
  virtual uint32_t read(char** p);
  uint32_t readAsS32(char **p); // depreciated
private:
  void* pWav_;
};

class Decoder_OGG : public Decoder
{
public:
  Decoder_OGG();
  virtual ~Decoder_OGG();
  virtual bool open(rutil::FileData &fd);
  virtual bool open(const char* p, size_t len);
  virtual void close();
  virtual uint32_t read(char** p);
private:
  void *pContext;
  char *buffer;
  int bytes;
};

class Decoder_LAME : public Decoder
{
public:
  Decoder_LAME();
  virtual ~Decoder_LAME();
  virtual bool open(rutil::FileData &fd);
  virtual bool open(const char* p, size_t len);
  virtual void close();
  virtual uint32_t read(char** p);
private:
  void *pContext_;
};

class Decoder_FLAC : public Decoder
{
public:
  Decoder_FLAC();
  virtual ~Decoder_FLAC();
  virtual bool open(rutil::FileData &fd);
  virtual bool open(const char* p, size_t len);
  virtual void close();
  virtual uint32_t read(char** p);

  rutil::FileData& get_fd();
  SoundInfo& info();
  uint32_t total_samples_;
  uint8_t* buffer_;
  size_t buffer_pos_;

private:
  void *pContext_;
  rutil::FileData fd_;
};

}

#endif
