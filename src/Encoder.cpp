#include "Encoder.h"
#include "Error.h"

namespace rhythmus
{

Encoder::Encoder(const PCMBuffer &sound)
{
  ASSERT(!sound.IsEmpty());
  info_ = sound.get_info();
  size_t byte_offset = 0;
  while (byte_offset < sound.get_total_byte())
  {
    size_t remains = 0;
    const int8_t* p = sound.AccessData(byte_offset, &remains);
    buffers_.emplace_back(BufferInfo{ p, remains });
    byte_offset += remains;
  }
  total_buffer_size_ = byte_offset;
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