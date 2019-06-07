#include "Encoder.h"
#include "Error.h"

namespace rhythmus
{

Encoder::Encoder(const Sound &sound)
{
  ASSERT(!sound.IsEmpty());
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
  metadata_[key] = { 0, 0, value };
}

void Encoder::SetMetadata(const std::string& key, int8_t* p, size_t s)
{
  metadata_[key] = { p, s, "" };
}

bool Encoder::IsMetaData(const std::string& key)
{
  auto ii = metadata_.find(key);
  return ii != metadata_.end();
}

bool Encoder::GetMetadata(const std::string& key, std::string& value)
{
  auto ii = metadata_.find(key);
  if (ii == metadata_.end()) return false;
  value = ii->second.s;
  return true;
}

bool Encoder::GetMetadata(const std::string& key, const int8_t** p, size_t& s)
{
  auto ii = metadata_.find(key);
  if (ii == metadata_.end()) return false;
  *p = ii->second.b.p;
  s = ii->second.b.s;
  return true;
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
  for (auto& ii : metadata_)
  {
    free(ii.second.b.p);
  }
  metadata_.clear();
}

}