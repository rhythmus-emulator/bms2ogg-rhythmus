#include "Encoder.h"

namespace rhythmus
{

Encoder::Encoder(const Sound &sound)
{
  info_ = sound.get_info();
  buffers_.emplace_back(BufferInfo{ sound.ptr(), sound.buffer_size() });
  total_buffer_size_ = sound.buffer_size();
}

Encoder::Encoder(const SoundMixer &mixer)
{
  info_ = mixer.get_info();
  total_buffer_size_ = 0;
  for (size_t i=0; i<mixer.get_chunk_count(); i++)
  {
    total_buffer_size_ += mixer.get_chunk_size();
    buffers_.emplace_back(BufferInfo{ mixer.get_chunk(i), mixer.get_chunk_size() });
  }
}

Encoder::~Encoder() { Close(); }

void Encoder::SetMetadata(const std::string& key, const std::string& value)
{
  metadata_[key] = value;
}

void Encoder::DeleteMetadata(const std::string& key)
{
  auto ii = metadata_.find(key);
  if (ii != metadata_.end())
    metadata_.erase(ii);
}

bool Encoder::Write(const std::string& path)
{
  return false;
}

void Encoder::Close() {}

const std::map<std::string, std::string>& Encoder::metadata() const { return metadata_; }

}