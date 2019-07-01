#ifndef RENCODER_ENCODER_H
#define RENCODER_ENCODER_H

#include <string>
#include <map>
#include <vector>
#include "Sound.h"

namespace rhythmus
{

class Encoder
{
public:
  Encoder(const PCMBuffer &sound);
  ~Encoder();

  void SetMetadata(const std::string& key, const std::string& value);
  void SetMetadata(const std::string& key, int8_t* p, size_t s);
  bool IsMetaData(const std::string& key);
  bool GetMetadata(const std::string& key, std::string& value);
  bool GetMetadata(const std::string& key, const int8_t** p, size_t& s);
  void DeleteMetadata(const std::string& key);
  void SetQuality(double quality);
  virtual bool Write(const std::string& path);
  virtual void Close();
protected:
  struct BufferInfo
  {
    const int8_t* p;
    size_t s;
  };
  struct MetaData
  {
    struct {
      int8_t* p;
      size_t s;
    } b;
    std::string s;
  };
  std::map<std::string, MetaData> metadata_;
  SoundInfo info_;
  std::vector<BufferInfo> buffers_;
  size_t total_buffer_size_;
  double quality_;
};

class Encoder_WAV : public Encoder
{
public:
  Encoder_WAV(const PCMBuffer& sound);
  virtual bool Write(const std::string& path);
};

class Encoder_OGG: public Encoder
{
public:
  Encoder_OGG(const PCMBuffer& sound);
  virtual bool Write(const std::string& path);
private:
  int quality_level;

  int current_buffer_index;
  int current_buffer_offset;
  void initbufferread();
  long bufferread(char* pOut, size_t size);
};

class Encoder_FLAC : public Encoder
{
public:
  Encoder_FLAC(const PCMBuffer& sound);
  virtual bool Write(const std::string& path);
};

}

#endif