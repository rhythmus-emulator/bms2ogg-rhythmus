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
  Encoder(const Sound &sound);
  Encoder(const SoundMixer &mixer);
  ~Encoder();
  void SetMetadata(const std::string& key, const std::string& value);
  void DeleteMetadata(const std::string& key);
  virtual bool Write(const std::string& path);
  virtual void Close();
  const std::map<std::string, std::string>& metadata() const;
protected:
  std::map<std::string, std::string> metadata_;
  SoundInfo info_;
  struct BufferInfo
  {
    const int8_t* p;
    size_t s;
  };
  std::vector<BufferInfo> buffers_;
  size_t total_buffer_size_;
};

class Encoder_WAV : public Encoder
{
public:
  Encoder_WAV(const Sound& sound);
  Encoder_WAV(const SoundMixer& mixer);
  virtual bool Write(const std::string& path);
};

}

#endif