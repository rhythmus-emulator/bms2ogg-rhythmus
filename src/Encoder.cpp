#include "Encoder.h"

namespace rhythmus
{

Encoder::Encoder(const Sound &sound)
{
  info_ = sound.get_info();
  buffers_.emplace_back(BufferInfo{ sound.ptr(), sound.buffer_byte_size() });
  total_buffer_size_ = sound.buffer_byte_size();
}

Encoder::Encoder(const SoundMixer &mixer)
{
  info_ = mixer.get_info();
  total_buffer_size_ = mixer.get_total_size();
  size_t remain_buffer_size = total_buffer_size_;
  for (size_t i=0; i<mixer.get_chunk_count(); i++)
  {
    size_t cur_chunk_size = remain_buffer_size;
    if (remain_buffer_size > mixer.get_chunk_size())
    {
      cur_chunk_size = mixer.get_chunk_size();
      remain_buffer_size -= mixer.get_chunk_size();
    }
    buffers_.emplace_back(BufferInfo{ mixer.get_chunk(i), cur_chunk_size });
  }
}

Encoder::~Encoder() { Close(); }

void Encoder::SetMetadata(const std::string& key, const std::string& value)
{
  metadata_[key] = value;
}

void Encoder::SetMetadata(const std::string& key, int8_t* p, size_t s)
{
  metadata_buffer_[key] = { p, s };
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

void Encoder::Close()
{
  metadata_.clear();
  for (auto& ii : metadata_buffer_)
  {
    free(const_cast<int8_t*>(ii.second.p));
  }
}

const std::map<std::string, std::string>& Encoder::metadata() const { return metadata_; }

const std::map<std::string, Encoder::BufferInfo>& Encoder::metadata_buffer() const { return metadata_buffer_; }

}