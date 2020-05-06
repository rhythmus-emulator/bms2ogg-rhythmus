#ifndef RMIXER_DECODER_H
#define RMIXER_DECODER_H

#include <stdint.h>
#include "rparser.h" /* due to rutil module */
#include "Sound.h"

namespace rmixer
{

/**
 * @brief Decode PCM from encoded file data.
 * @warn  Decoded PCM data always given in form of 8/16/32bit sampling.
 */
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
  uint32_t read(char** p);
  uint32_t readWithFormat(char** p, const SoundInfo& info);
  const SoundInfo& get_info();

protected:
  SoundInfo info_;
  virtual uint32_t read_internal(char** p, bool read_raw) = 0;
};

class Decoder_WAV : public Decoder
{
public:
  Decoder_WAV();
  virtual ~Decoder_WAV();
  virtual bool open(rutil::FileData &fd);
  virtual bool open(const char* p, size_t len);
  virtual void close();
  uint32_t readAsS32(char **p); // deprecated
private:
  virtual uint32_t read_internal(char** p, bool read_raw);
  unsigned original_bps_;
  bool is_compressed_;
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
private:
  virtual uint32_t read_internal(char** p, bool read_raw);
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
private:
  virtual uint32_t read_internal(char** p, bool read_raw);
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

  rutil::FileData& get_fd();
  SoundInfo& info();
  uint64_t total_samples_;
  uint8_t* buffer_;
  size_t buffer_pos_;

private:
  virtual uint32_t read_internal(char** p, bool read_raw);
  void *pContext_;
  rutil::FileData fd_;
};

}

#endif
