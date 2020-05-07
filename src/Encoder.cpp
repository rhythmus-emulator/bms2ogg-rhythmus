#include "Encoder.h"
#include "Error.h"

namespace rmixer
{

Encoder::Encoder(const Sound &sound)
  : curr_sound_(&sound), quality_(0.6)
{
  CreateBufferListFromSound();
}

void Encoder::CreateBufferListFromSound()
{
  RMIXER_ASSERT(curr_sound_);
  info_ = curr_sound_->get_soundinfo();
  size_t byte_offset = 0;
  size_t total_byte = curr_sound_->get_total_byte();
  buffers_.clear();
  buffers_.emplace_back(BufferInfo{ curr_sound_->get_ptr(), curr_sound_->get_total_byte() });
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

void Encoder::SetQuality(double quality)
{
  quality_ = quality;
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

bool Encoder::Write(const std::string& path, const SoundInfo &soundinfo)
{
  if (!curr_sound_) return false;
  if (curr_sound_->get_soundinfo() == soundinfo) return Write(path);
  const Sound *old_sound = curr_sound_;
  bool r = false;
  Sound new_sound;
  new_sound.copy(*curr_sound_);
  if (!new_sound.Resample(soundinfo)) return false;

  // temporary exchange sound data pointer
  // XXX: cannot be recovered if exception thrown..?
  curr_sound_ = &new_sound;
  CreateBufferListFromSound();
  r = Write(path);

  // recover and finish
  curr_sound_ = old_sound;
  CreateBufferListFromSound();
  return r;
}

void Encoder::Close()
{
  // XXX: is it corrent that metadata should be deleted by Encoder?
  for (auto& ii : metadata_)
  {
    free(ii.second.b.p);
  }
  metadata_.clear();
}

}