#include "Sound.h"
#include "Midi.h"
#include "Error.h"
#include "Decoder.h"
#include "Encoder.h"
#include "Sampler.h"

// for sending timidity event
#include "playmidi.h"

namespace rmixer
{

template <typename T>
void MixSampleWithClipping(T* dest, T* source)
{
  // for fast processing
  if (*dest == 0)
    *dest = *source;
  else if (*dest > 0 && *dest > std::numeric_limits<T>::max() - *source)
    *dest = std::numeric_limits<T>::max();
  else if (*dest < 0 && *dest < std::numeric_limits<T>::min() - *source)
    *dest = std::numeric_limits<T>::min();
  else
    *dest += *source;
}

template <typename T>
void MixSampleWithClipping(T* dest, T* source, float src_volume)
{
  T source2 = static_cast<T>((*source) * src_volume);
  MixSampleWithClipping(dest, &source2);
}

void memmix(int8_t* dst, const int8_t* src, size_t bytesize, size_t bytepersample, float src_volume)
{
  if (bytesize == 0) return;
  size_t i = 0;
  if (bytepersample == 1)
  {
    // first do code unrolled sample mixing
    for (; i < bytesize - 4; i += 4)
    {
      MixSampleWithClipping((int8_t*)(dst + i), (int8_t*)(src + i), src_volume);
      MixSampleWithClipping((int8_t*)(dst + i + 1), (int8_t*)(src + i + 1), src_volume);
      MixSampleWithClipping((int8_t*)(dst + i + 2), (int8_t*)(src + i + 2), src_volume);
      MixSampleWithClipping((int8_t*)(dst + i + 3), (int8_t*)(src + i + 3), src_volume);
    }
    // do left
    for (; i < bytesize; ++i)
    {
      MixSampleWithClipping((int8_t*)(dst + i), (int8_t*)(src + i), src_volume);
    }
  }
  else if (bytepersample == 2)
  {
    // first do code unrolled sample mixing
    for (; i < bytesize - 8; i += 8)
    {
      MixSampleWithClipping((int16_t*)(dst + i), (int16_t*)(src + i), src_volume);
      MixSampleWithClipping((int16_t*)(dst + i + 2), (int16_t*)(src + i + 2), src_volume);
      MixSampleWithClipping((int16_t*)(dst + i + 4), (int16_t*)(src + i + 4), src_volume);
      MixSampleWithClipping((int16_t*)(dst + i + 6), (int16_t*)(src + i + 6), src_volume);
    }
    // do left
    for (; i < bytesize; i += 2)
    {
      MixSampleWithClipping((int16_t*)(dst + i), (int16_t*)(src + i), src_volume);
    }
  }
  else if (bytepersample == 4)
  {
    // first do code unrolled sample mixing
    for (; i < bytesize - 16; i += 16)
    {
      MixSampleWithClipping((int32_t*)(dst + i), (int32_t*)(src + i), src_volume);
      MixSampleWithClipping((int32_t*)(dst + i + 4), (int32_t*)(src + i + 4), src_volume);
      MixSampleWithClipping((int32_t*)(dst + i + 8), (int32_t*)(src + i + 8), src_volume);
      MixSampleWithClipping((int32_t*)(dst + i + 12), (int32_t*)(src + i + 12), src_volume);
    }
    // do left
    for (; i < bytesize; i += 4)
    {
      MixSampleWithClipping((int32_t*)(dst + i), (int32_t*)(src + i), src_volume);
    }
  }
  else ASSERT(0);
}
void memmix(int8_t* dst, const int8_t* src, size_t bytesize, size_t bytepersample)
{
  if (bytesize == 0) return;
  size_t i = 0;
  if (bytepersample == 1)
  {
    // first do code unrolled sample mixing
    for (; i < bytesize - 4; i += 4)
    {
      MixSampleWithClipping((int8_t*)(dst + i), (int8_t*)(src + i));
      MixSampleWithClipping((int8_t*)(dst + i + 1), (int8_t*)(src + i + 1));
      MixSampleWithClipping((int8_t*)(dst + i + 2), (int8_t*)(src + i + 2));
      MixSampleWithClipping((int8_t*)(dst + i + 3), (int8_t*)(src + i + 3));
    }
    // do left
    for (; i < bytesize; ++i)
    {
      MixSampleWithClipping((int8_t*)(dst + i), (int8_t*)(src + i));
    }
  }
  else if (bytepersample == 2)
  {
    // first do code unrolled sample mixing
    for (; i < bytesize - 8; i += 8)
    {
      MixSampleWithClipping((int16_t*)(dst + i), (int16_t*)(src + i));
      MixSampleWithClipping((int16_t*)(dst + i + 2), (int16_t*)(src + i + 2));
      MixSampleWithClipping((int16_t*)(dst + i + 4), (int16_t*)(src + i + 4));
      MixSampleWithClipping((int16_t*)(dst + i + 6), (int16_t*)(src + i + 6));
    }
    // do left
    for (; i < bytesize; i += 2)
    {
      MixSampleWithClipping((int16_t*)(dst + i), (int16_t*)(src + i));
    }
  }
  else if (bytepersample == 4)
  {
    // first do code unrolled sample mixing
    for (; i < bytesize - 16; i += 16)
    {
      MixSampleWithClipping((int32_t*)(dst + i), (int32_t*)(src + i));
      MixSampleWithClipping((int32_t*)(dst + i + 4), (int32_t*)(src + i + 4));
      MixSampleWithClipping((int32_t*)(dst + i + 8), (int32_t*)(src + i + 8));
      MixSampleWithClipping((int32_t*)(dst + i + 12), (int32_t*)(src + i + 12));
    }
    // do left
    for (; i < bytesize; i += 4)
    {
      MixSampleWithClipping((int32_t*)(dst + i), (int32_t*)(src + i));
    }
  }
  else ASSERT(0);
}

bool operator==(const SoundInfo& a, const SoundInfo& b)
{
  return a.bitsize == b.bitsize &&
         a.rate == b.rate &&
         a.channels == b.channels;
}

bool operator!=(const SoundInfo& a, const SoundInfo& b)
{
  return !(a == b);
}

uint32_t GetByteFromFrame(uint32_t frame, const SoundInfo& info)
{
  return frame * info.channels * info.bitsize / 8;
}

uint32_t GetByteFromMilisecond(uint32_t ms, const SoundInfo& info)
{
  return GetByteFromFrame(GetFrameFromMilisecond(ms, info), info);
}

uint32_t GetFrameFromMilisecond(uint32_t ms, const SoundInfo& info)
{
  return static_cast<uint32_t>(info.rate / 1000.0 * ms);
}

uint32_t GetFrameFromByte(uint32_t byte, const SoundInfo& info)
{
  return byte * 8 / info.bitsize / info.channels;
}

uint32_t GetMilisecondFromByte(uint32_t byte, const SoundInfo& info)
{
  return static_cast<uint32_t>(GetFrameFromByte(byte, info) * 1000.0 / info.rate);
}


// ---------------------------------- SoundInfo

/* Global-scope default sound info */
SoundInfo g_soundinfo(16, 2, 44100);

SoundInfo::SoundInfo()
  : bitsize(g_soundinfo.bitsize), channels(g_soundinfo.channels), rate(g_soundinfo.rate)
{}

SoundInfo::SoundInfo(uint16_t bitsize_, uint8_t channels_, uint32_t rate_)
  : bitsize(bitsize_), channels(channels_), rate(rate_) {}

void SoundInfo::SetDefaultSoundInfo(const SoundInfo& info)
{
  g_soundinfo = info;
}

const SoundInfo& SoundInfo::GetDefaultSoundInfo()
{
  return g_soundinfo;
}

// ---------------------------- class PCMBuffer

PCMBuffer::PCMBuffer() : buffer_size_(0), buffer_(0)
{
  memset(&info_, 0, sizeof(SoundInfo));
}

PCMBuffer::PCMBuffer(const SoundInfo& info, size_t buffer_size)
{
  AllocateSize(info, buffer_size);
}

PCMBuffer::PCMBuffer(const SoundInfo& info, size_t buffer_size, int8_t *p)
{
  info_ = info;
  buffer_size_ = buffer_size;
  buffer_ = p;
}

PCMBuffer::PCMBuffer(const PCMBuffer &buf)
{
  info_ = buf.info_;
  buffer_size_ = buf.buffer_size_;
  buffer_ = (int8_t*)malloc(buffer_size_);
  memcpy(buffer_, buf.buffer_, buffer_size_);
}

PCMBuffer::PCMBuffer(PCMBuffer &&buf)
{
  info_ = buf.info_;
  buffer_size_ = buf.buffer_size_;
  buffer_ = buf.buffer_;
  buf.buffer_ = 0;
  buf.buffer_size_ = 0;
}

PCMBuffer& PCMBuffer::operator=(PCMBuffer&& buf)
{
  PCMBuffer::PCMBuffer(buf);
  return *this;
}

PCMBuffer::~PCMBuffer()
{
  Clear();
}

void PCMBuffer::Clear()
{
  if (buffer_)
  {
    free(buffer_);
    buffer_ = 0;
    buffer_size_ = 0;
  }
}

bool PCMBuffer::Resample(const SoundInfo& info)
{
  // resampling if necessary
  if (get_info() != info && !IsEmpty())
  {
    PCMBuffer *new_s = new PCMBuffer();
    Sampler sampler(*this, info);
    if (!sampler.Resample(*new_s))
      return false;
    std::swap(*this, *new_s);
  }
  info_ = info;
  return true;
}

bool PCMBuffer::Resample(double pitch, double tempo, double volume)
{
  // resampling for pitch / speed / etc.
  // sound quality is not changed by this method.
  PCMBuffer *new_s = new PCMBuffer();
  Sampler sampler(*this, info_);
  sampler.SetPitch(pitch);
  sampler.SetTempo(tempo);
  sampler.SetVolume(volume);
  if (!sampler.Resample(*new_s))
    return false;
  std::swap(*this, *new_s);
  return true;
}

void PCMBuffer::AllocateSize(const SoundInfo& info, size_t buffer_size)
{
  Clear();
  info_ = info;
  buffer_size_ = buffer_size;
  buffer_ = (int8_t*)calloc(1, buffer_size);
}

void PCMBuffer::AllocateDuration(const SoundInfo& info, uint32_t duration_ms)
{
  AllocateSize(info, GetByteFromMilisecond(duration_ms, info));
}

void PCMBuffer::SetBuffer(const SoundInfo& info, size_t framecount, void *p)
{
  Clear();
  info_ = info;
  buffer_ = (int8_t*)p;
}

void PCMBuffer::SetEmptyBuffer(const SoundInfo& info, size_t framecount)
{
  AllocateSize(info, info.channels * info.bitsize / 8 * framecount);
}

const SoundInfo& PCMBuffer::get_info() const
{
  return info_;
}

bool PCMBuffer::IsEmpty() const
{
  return buffer_size_ == 0;
}

size_t PCMBuffer::get_total_byte() const
{
  return buffer_size_;
}

int8_t* PCMBuffer::get_ptr()
{
  return buffer_;
}

const int8_t* PCMBuffer::get_ptr() const
{
  return buffer_;
}

size_t PCMBuffer::GetFrameCount() const
{
  return buffer_size_ * 8 / info_.bitsize / info_.channels;
}

uint32_t PCMBuffer::GetDurationInMilisecond() const
{
  return GetMilisecondFromByte(buffer_size_, info_);
}


// ---------------------------- class BaseSound

BaseSound::BaseSound()
  : volume_(1.0f), loop_(false)
{}

void BaseSound::SetVolume(float v)
{
  volume_ = v;
}

void BaseSound::SetLoop(bool loop)
{
  loop_ = loop;
}

void BaseSound::SetId(const std::string& id)
{
  id_ = id;
}

const std::string& BaseSound::GetId()
{
  return id_;
}

size_t BaseSound::MixDataTo(int8_t* copy_to, size_t byte_len) const
{
  return 0;
}

size_t BaseSound::MixDataFrom(int8_t* copy_from, size_t src_offset, size_t byte_len) const
{
  return 0;
}



// -------------------------------- class Sound

Sound::Sound() : PCMBuffer()
{
  memset(&info_, 0, sizeof(SoundInfo));
}

Sound::Sound(const SoundInfo& info, size_t framecount)
  : PCMBuffer(info, info.channels * info.bitsize * framecount / 8),
    buffer_remain_(0)
{
}

Sound::Sound(const SoundInfo& info, size_t framecount, void *p)
  : PCMBuffer(info, info.channels * info.bitsize * framecount / 8, (int8_t*)p),
    buffer_remain_(0)
{
}

Sound::~Sound()
{
  Clear();
}

bool Sound::Load(const std::string& path)
{
  rutil::FileData fd;
  rutil::ReadFileData(path, fd);
  if (fd.IsEmpty())
    return false;
  return Load((char*)fd.p, fd.len, rutil::lower(rutil::GetExtension(path)));
}

bool Sound::Load(const std::string& path, const SoundInfo& info)
{
  if (!Load(path))
    return false;

  return Resample(info);
}

bool Sound::Load(const char* p, size_t len, const std::string& ext)
{
  Decoder *decoder = nullptr;
  bool r = false;
  size_t buflen = 0;
  char *buf = 0;

  if (ext == "wav") decoder = new Decoder_WAV();
  else if (ext == "ogg") decoder = new Decoder_OGG();
  else if (ext == "flac") decoder = new Decoder_FLAC();
  else if (ext == "mp3") decoder = new Decoder_LAME();

  if (!decoder)
    return false;

  r = (decoder->open(p, len) && (buflen = decoder->read(&buf)) != 0);
  delete decoder;
  if (!r)
  {
    // failure cleanup
    if (buf) free(buf);
  }
  else
  {
    // set buffer
    SetBuffer(info_, GetFrameFromByte(buflen, info_), buf);
  }
  return r;
}

bool Sound::Save(const std::string& path)
{
  std::map<std::string, std::string> __;
  return Save(path, __, 0.6);
}

bool Sound::Save(const std::string& path,
  const std::map<std::string, std::string> &metadata,
  double quality)
{
  Encoder *encoder = nullptr;
  std::string ext = rutil::lower(rutil::GetExtension(path));
  bool r = false;

  if (ext == "WAV")
    encoder = new Encoder_WAV(*this);
  else if (ext == "OGG")
    encoder = new Encoder_OGG(*this);
  else if (ext == "FLAC")
    encoder = new Encoder_FLAC(*this);

  if (!encoder)
    return false;
  else
  {
    for (auto &i : metadata)
      encoder->SetMetadata(i.first, i.second);
    encoder->SetQuality(quality);
    r = encoder->Write(path);
    delete encoder;
    return r;
  }
}

size_t Sound::MixDataTo(int8_t* copy_to, size_t desired_byte) const
{
  if (buffer_remain_ >= buffer_size_)
    return 0;
  size_t offset = buffer_size_ - buffer_remain_;
  if (desired_byte + offset > buffer_size_)
    desired_byte = buffer_size_ - offset;
  //if (copy && volume == 1.0f) memcpy(copy_to, buffer_ + offset, desired_byte);
  if (volume_ == 1.0f) memmix(copy_to, buffer_ + offset, desired_byte, info_.bitsize / 8);
  else memmix(copy_to, buffer_ + offset, desired_byte, info_.bitsize / 8, volume_);
  return desired_byte;
}

void Sound::Play(int)
{
  buffer_remain_ = buffer_size_;
}

void Sound::Stop(int)
{
  buffer_remain_ = 0;
}



SoundMidi::SoundMidi()
  : midi_(nullptr), midi_channel_(0)
{

}

void SoundMidi::SetMidi(Midi* midi)
{
  midi_ = midi;
}

void SoundMidi::SetMidiChannel(int midi_channel)
{
  midi_channel_ = midi_channel;
}

void SoundMidi::Play(int key)
{
  if (!midi_) return;
  midi_->SendEvent(
    ME_NOTEON, midi_channel_, (uint8_t)key, (uint8_t)(0x7F * volume_) /* Velo */
  );
}

void SoundMidi::Stop(int key)
{
  if (!midi_) return;
  midi_->SendEvent(
    ME_NOTEOFF, (uint8_t)midi_channel_, (uint8_t)key, 0
  );
}

void SoundMidi::SendEvent(uint8_t arg1, uint8_t arg2, uint8_t arg3)
{
  if (!midi_) return;
  midi_->SendEvent(
    arg1, midi_channel_, arg2, arg3
  );
}



#if 0
SoundVariableBuffer::SoundVariableBuffer(const SoundInfo& info, size_t chunk_byte_size)
  : PCMBuffer(info, 0), chunk_byte_size_(chunk_byte_size),
    frame_count_in_chunk_(GetFrameFromByte(chunk_byte_size, info))
{
}

SoundVariableBuffer::~SoundVariableBuffer()
{
  Clear();
}

SoundVariableBuffer& SoundVariableBuffer::operator=(SoundVariableBuffer &&s)
{
  std::swap(buffers_, s.buffers_);
  buffer_size_ = s.buffer_size_;
  chunk_byte_size_ = s.chunk_byte_size_;
  frame_count_in_chunk_ = s.frame_count_in_chunk_;

  s.buffers_.clear();
  s.buffer_size_ = 0;
  return *this;
}

size_t SoundVariableBuffer::Mix(size_t ms, const PCMBuffer& s, float volume)
{
  // only mixing with same type of sound data
  if (s.get_info() != info_) return 0;

  uint32_t totalwritesize = s.get_total_byte();
  uint32_t startoffset = GetByteFromMilisecond(ms, info_);
  uint32_t endoffset = startoffset + totalwritesize;
  PrepareByteoffset(endoffset);
  size_t chunk_idx = startoffset / chunk_byte_size_;
  size_t chunk_endpos = chunk_byte_size_ * (chunk_idx + 1);
  size_t writtensize = 0;
  while (writtensize < s.get_total_byte())
  {
    const size_t dest_buf_offset = (startoffset + writtensize) % chunk_byte_size_;
    const size_t writingsize = 
      totalwritesize > (chunk_byte_size_ - dest_buf_offset)
      ? (chunk_byte_size_ - dest_buf_offset)
      : totalwritesize;
    size_t mixedlen = s.MixData(buffers_[chunk_idx] + dest_buf_offset, writtensize, writingsize, false, volume);
    DASSERT(mixedlen == writingsize);  // currently only supports Fixed Sound buffer
    writtensize += writingsize;
    totalwritesize -= writingsize;
    chunk_endpos += chunk_byte_size_;
    chunk_idx++;
  }

  return writtensize;
}

size_t SoundVariableBuffer::MixData(int8_t* copy_to, size_t offset, size_t desired_byte, bool copy, float volume) const
{
  size_t max_copy_byte;
  const int8_t* p = AccessData(offset, &max_copy_byte);
  if (!p) return 0;

  if (copy && volume == 1.0f) memcpy(copy_to, p, desired_byte);
  else if (volume == 1.0f) memmix(copy_to, p, desired_byte, info_.bitsize / 8);
  else memmix(copy_to, p, desired_byte, info_.bitsize / 8, volume);
  return desired_byte;
}

void SoundVariableBuffer::PrepareByteoffset(uint32_t offset)
{
  if (buffer_size_ > offset) return;
  else
  {
    buffer_size_ = offset;
    size_t chunk_necessary_count = offset / chunk_byte_size_;
    if (chunk_byte_size_ % offset > 0)
      chunk_necessary_count++;
    while (buffers_.size() < chunk_necessary_count)
    {
      buffers_.push_back((int8_t*)malloc(chunk_byte_size_));
      memset(buffers_.back(), 0, chunk_byte_size_);
    }
  }
}

int8_t* SoundVariableBuffer::AccessData(size_t byte_offset, size_t* remaining_byte)
{
  if (byte_offset > buffer_size_) return 0;
  size_t chunk_idx = byte_offset / chunk_byte_size_;
  size_t byte_offset_in_chunk = byte_offset % chunk_byte_size_;
  // remaining byte : available(writable) byte in returned ptr.
  if (remaining_byte)
    *remaining_byte = chunk_byte_size_ - byte_offset_in_chunk;
  return buffers_[chunk_idx] + byte_offset_in_chunk;
}

void SoundVariableBuffer::Clear()
{
  for (auto *p : buffers_)
    free(p);
  buffers_.clear();
  buffer_size_ = 0;
}

int8_t* SoundVariableBuffer::get_chunk(int idx)
{
  if (idx < buffers_.size())
    return buffers_[idx];
  else return nullptr;
}

const int8_t* SoundVariableBuffer::get_chunk(int idx) const
{
  return const_cast<SoundVariableBuffer*>(this)->get_chunk(idx);
}

void SoundVariableBufferToSoundBuffer(SoundVariableBuffer &in, Sound &out)
{
  Sound s(in.get_info(), in.GetFrameCount());
  const int8_t *buf = 0;
  size_t pos = 0, rem = 0;
  while ((buf = in.AccessData(pos, &rem)) && rem > 0)
  {
    memcpy(s.ptr() + pos, buf, rem);
    pos += rem;
  }
  std::swap(out, s);
}
#endif

}