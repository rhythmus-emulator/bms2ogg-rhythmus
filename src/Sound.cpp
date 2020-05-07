#include "Sound.h"
#include "Midi.h"
#include "Error.h"
#include "Mixer.h"
#include "Decoder.h"
#include "Encoder.h"
#include "Sampler.h"
#include "Effector.h"


#if __BYTE_ORDER == __LITTLE_ENDIAN
# define LITTLE_ENDIAN 1
#elif __BYTE_ORDER == __BIG_ENDIAN
# define BIG_ENDIAN 1
#else
# define LITTLE_ENDIAN 1
#endif


namespace rmixer
{

static bool enable_detailed_log = false;

// mixing util function start

template <typename T>
void MixSampleWithClipping(T* dest, const T* source)
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
void MixSampleWithClipping(T* dest, const T* source, float src_volume)
{
  T source2;
  if (std::numeric_limits<T>::max() / (float)src_volume < (float)(*source))
    source2 = std::numeric_limits<T>::max();
  else if (*source < 0 && std::numeric_limits<T>::min() / (float)src_volume > (float)(*source))
    source2 = std::numeric_limits<T>::min();  /* negative check only for signed type */
  else
    source2 = static_cast<T>((*source) * src_volume);
  *dest += *source;
}

static inline int32_t I24toI32(const int8_t* source)
{
  return (*((int32_t*)source) & 0x00ffffff) << 8;
}

// XXX: is it correct for LE/BE?
void Mix24SampleWithClipping(int8_t* dest, const int8_t* source)
{
  int32_t source2 = (*((int32_t*)source) & 0x00ffffff);
  int32_t dest2 = (*((int32_t*)dest) & 0x00ffffff);
  if (source2 < 0x007fffff && source2 > (0x007fffff - dest2))
  {
#ifdef LITTLE_ENDIAN
    *(char*)dest = (char)0xff;
    *(char*)(dest + 1) = (char)0xff;
    *(char*)(dest + 2) = (char)0x7f;
#else
    *(char*)dest = (char)0x7f;
    *(char*)(dest + 1) = (char)0xff;
    *(char*)(dest + 2) = (char)0xff;
#endif
  }
  else if (source2 > (0x00ffffff - dest2))
  {
    *(char*)dest = (char)0xff;
    *(char*)(dest + 1) = (char)0xff;
    *(char*)(dest + 2) = (char)0xff;
  }
  else
  {
#ifdef LITTLE_ENDIAN
    int32_t r = source2 + dest2;
    *dest = *((int8_t*)&r + 3);
    *(dest + 1) = *((int8_t*)&r + 2);
    *(dest + 2) = *((int8_t*)&r + 1);
#else
    int32_t r = source2 + dest2;
    *dest = *((int8_t*)&r);
    *(dest + 1) = *((int8_t*)&r + 1);
    *(dest + 2) = *((int8_t*)&r + 2);
#endif
  }
}

void Mix24SampleWithClipping(int8_t* dest, const int8_t* source, float src_volume)
{
  int32_t source2 = (*((int32_t*)source) & 0x00ffffff);
  int32_t dest2 = (*((int32_t*)dest) & 0x00ffffff);
  if (source2 < 0x007fffff && source2 > (0x007fffff - dest2) / src_volume)
  {
#ifdef LITTLE_ENDIAN
    *(char*)dest = (char)0xff;
    *(char*)(dest + 1) = (char)0xff;
    *(char*)(dest + 2) = (char)0x7f;
#else
    *(char*)dest = (char)0x7f;
    *(char*)(dest + 1) = (char)0xff;
    *(char*)(dest + 2) = (char)0xff;
#endif
  }
  else if (source2 > (0x00ffffff - dest2) / src_volume)
  {
    *(char*)dest = (char)0xff;
    *(char*)(dest + 1) = (char)0xff;
    *(char*)(dest + 2) = (char)0xff;
  }
  else
  {
#ifdef LITTLE_ENDIAN
    int32_t r = source2 + dest2;
    *dest = *((int8_t*)&r + 1);
    *(dest + 1) = *((int8_t*)&r + 2);
    *(dest + 2) = *((int8_t*)&r + 3);
#else
    int32_t r = source2 + dest2;
    *dest = *((int8_t*)&r);
    *(dest + 1) = *((int8_t*)&r + 1);
    *(dest + 2) = *((int8_t*)&r + 2);
#endif
  }
}

void pcmcpy(int8_t* dst, const int8_t* src, size_t sample_count)
{
  memcpy(dst, src, sizeof(int8_t) * sample_count);
}

void pcmcpy(int16_t* dst, const int16_t* src, size_t sample_count)
{
  memcpy(dst, src, sizeof(int16_t) * sample_count);
}

void pcmcpy24(int8_t* dst, const int8_t* src, size_t sample_count)
{
  memcpy(dst, src, 3 * sample_count);
}

void pcmcpy(int32_t* dst, const int32_t* src, size_t sample_count)
{
  memcpy(dst, src, sizeof(int32_t) * sample_count);
}

void pcmcpy(uint8_t* dst, const uint8_t* src, size_t sample_count)
{
  memcpy(dst, src, sizeof(uint8_t) * sample_count);
}

void pcmcpy(uint16_t* dst, const uint16_t* src, size_t sample_count)
{
  memcpy(dst, src, sizeof(uint16_t) * sample_count);
}

void pcmcpy(uint32_t* dst, const uint32_t* src, size_t sample_count)
{
  memcpy(dst, src, sizeof(uint32_t) * sample_count);
}

void pcmcpy(float* dst, const float* src, size_t sample_count)
{
  memcpy(dst, src, sizeof(float) * sample_count);
}

template <typename T>
void pcmmix_template(T* dst, const T* src, size_t sample_count)
{
  // first do code unrolled sample mixing
  size_t i = 0;
  for (; i < sample_count - 4; i += 4)
  {
    MixSampleWithClipping(dst + i, src + i);
    MixSampleWithClipping(dst + i + 1, src + i + 1);
    MixSampleWithClipping(dst + i + 2, src + i + 2);
    MixSampleWithClipping(dst + i + 3, src + i + 3);
  }
  // do left
  for (; i < sample_count; ++i)
  {
    MixSampleWithClipping(dst + i, src + i);
  }
}

void pcmmix(int8_t* dst, const int8_t* src, size_t sample_count)
{
  pcmmix_template(dst, src, sample_count);
}

void pcmmix(int16_t* dst, const int16_t* src, size_t sample_count)
{
  pcmmix_template(dst, src, sample_count);
}

void pcmmix24(int8_t* dst, const int8_t* src, size_t sample_count)
{
  // first do code unrolled sample mixing
  size_t i = 0;
  for (; i < sample_count - 4; i += 4)
  {
    Mix24SampleWithClipping(dst + i, src + i);
    Mix24SampleWithClipping(dst + i + 1, src + i + 1);
    Mix24SampleWithClipping(dst + i + 2, src + i + 2);
    Mix24SampleWithClipping(dst + i + 3, src + i + 3);
  }
  // do left
  for (; i < sample_count; ++i)
  {
    Mix24SampleWithClipping(dst + i, src + i);
  }
}

void pcmmix(int32_t* dst, const int32_t* src, size_t sample_count)
{
  pcmmix_template(dst, src, sample_count);
}

void pcmmix(uint8_t* dst, const uint8_t* src, size_t sample_count)
{
  pcmmix_template(dst, src, sample_count);
}

void pcmmix(uint16_t* dst, const uint16_t* src, size_t sample_count)
{
  pcmmix_template(dst, src, sample_count);
}

void pcmmix(uint32_t* dst, const uint32_t* src, size_t sample_count)
{
  pcmmix_template(dst, src, sample_count);
}

void pcmmix(float* dst, const float* src, size_t sample_count)
{
  pcmmix_template(dst, src, sample_count);
}

template <typename T>
void pcmcpy_vol_template(T* dst, const T* src, size_t sample_count, float volume)
{
  // first do code unrolled sample mixing
  size_t i = 0;
  for (; i < sample_count - 4; i += 4)
  {
    *(dst + i) = (T)(*(src + i) * volume);
    *(dst + i + 1) = (T)(*(src + i + 1) * volume);
    *(dst + i + 2) = (T)(*(src + i + 2) * volume);
    *(dst + i + 3) = (T)(*(src + i + 3) * volume);
  }
  // do left
  for (; i < sample_count; ++i)
  {
    *(dst + i) = (T)(*(src + i) * volume);
  }
}

void pcmcpy(int8_t* dst, const int8_t* src, size_t sample_count, float volume)
{
  pcmcpy_vol_template(dst, src, sample_count, volume);
}

void pcmcpy(int16_t* dst, const int16_t* src, size_t sample_count, float volume)
{
  pcmcpy_vol_template(dst, src, sample_count, volume);
}

void pcmcpy24(int8_t* dst, const int8_t* src, size_t sample_count, float volume)
{
  // XXX: slow operation, need to make it assigning?
  memset(dst, 0, sample_count * 3);
  pcmmix24(dst, src, sample_count, volume);
}

void pcmcpy(int32_t* dst, const int32_t* src, size_t sample_count, float volume)
{
  pcmcpy_vol_template(dst, src, sample_count, volume);
}

void pcmcpy(uint8_t* dst, const uint8_t* src, size_t sample_count, float volume)
{
  pcmcpy_vol_template(dst, src, sample_count, volume);
}

void pcmcpy(uint16_t* dst, const uint16_t* src, size_t sample_count, float volume)
{
  pcmcpy_vol_template(dst, src, sample_count, volume);
}

void pcmcpy(uint32_t* dst, const uint32_t* src, size_t sample_count, float volume)
{
  pcmcpy_vol_template(dst, src, sample_count, volume);
}

void pcmcpy(float* dst, const float* src, size_t sample_count, float volume)
{
  pcmcpy_vol_template(dst, src, sample_count, volume);
}

template <typename T>
void pcmmix_vol_template(T* dst, const T* src, size_t sample_count, float volume)
{
  // first do code unrolled sample mixing
  size_t i = 0;
  for (; i < sample_count - 4; i += 4)
  {
    MixSampleWithClipping(dst + i, src + i, volume);
    MixSampleWithClipping(dst + i + 1, src + i + 1, volume);
    MixSampleWithClipping(dst + i + 2, src + i + 2, volume);
    MixSampleWithClipping(dst + i + 3, src + i + 3, volume);
  }
  // do left
  for (; i < sample_count; ++i)
  {
    MixSampleWithClipping(dst + i, src + i, volume);
  }
}

void pcmmix(int8_t* dst, const int8_t* src, size_t sample_count, float volume)
{
  pcmmix_vol_template(dst, src, sample_count, volume);
}

void pcmmix(int16_t* dst, const int16_t* src, size_t sample_count, float volume)
{
  pcmmix_vol_template(dst, src, sample_count, volume);
}

void pcmmix24(int8_t* dst, const int8_t* src, size_t sample_count, float volume)
{
  for (size_t i = 0; i < sample_count; ++i)
    Mix24SampleWithClipping(dst + i * 3, src + i * 3, volume);
}

void pcmmix(int32_t* dst, const int32_t* src, size_t sample_count, float volume)
{
  pcmmix_vol_template(dst, src, sample_count, volume);
}

void pcmmix(uint8_t* dst, const uint8_t* src, size_t sample_count, float volume)
{
  pcmmix_vol_template(dst, src, sample_count, volume);
}

void pcmmix(uint16_t* dst, const uint16_t* src, size_t sample_count, float volume)
{
  pcmmix_vol_template(dst, src, sample_count, volume);
}

void pcmmix(uint32_t* dst, const uint32_t* src, size_t sample_count, float volume)
{
  pcmmix_vol_template(dst, src, sample_count, volume);
}

void pcmmix(float* dst, const float* src, size_t sample_count, float volume)
{
  pcmmix_vol_template(dst, src, sample_count, volume);
}


void pcmmix(int8_t* dst, const int8_t* src, size_t bytesize, size_t bytepersample, float src_volume)
{
  if (bytesize == 0) return;
  size_t i = 0;
  if (bytepersample == 1)
  {
    pcmmix((int16_t*)dst, (int16_t*)src, bytesize, src_volume);
  }
  else if (bytepersample == 2)
  {
    pcmmix((int16_t*)dst, (int16_t*)src, bytesize / 2, src_volume);
  }
  else if (bytepersample == 3)
  {
    pcmmix24((int8_t*)dst, (int8_t*)src, bytesize / 3, src_volume);
  }
  else if (bytepersample == 4)
  {
    pcmmix((int32_t*)dst, (int32_t*)src, bytesize / 4, src_volume);
  }
  else RMIXER_ASSERT(0);
}

void pcmmix(int8_t* dst, const int8_t* src, size_t bytesize, size_t bytepersample)
{
  if (bytesize == 0) return;
  size_t i = 0;
  if (bytepersample == 1)
  {
    pcmmix((int8_t*)dst, (int8_t*)src, bytesize);
  }
  else if (bytepersample == 2)
  {
    pcmmix((int16_t*)dst, (int16_t*)src, bytesize / 2);
  }
  else if (bytepersample == 3)
  {
    pcmmix24((int8_t*)dst, (int8_t*)src, bytesize / 3);
  }
  else if (bytepersample == 4)
  {
    pcmmix((int32_t*)dst, (int32_t*)src, bytesize / 4);
  }
  else RMIXER_ASSERT(0);
}

// mixing util function end


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

uint32_t GetByteFromSample(uint32_t sample, const SoundInfo& info)
{
  return sample * info.bitsize / 8;
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

uint32_t GetSampleFromByte(uint32_t byte, const SoundInfo& info)
{
  return byte * 8 / info.bitsize;
}

uint32_t GetMilisecondFromByte(uint32_t byte, const SoundInfo& info)
{
  return static_cast<uint32_t>(GetFrameFromByte(byte, info) * 1000.0 / info.rate);
}

float GetMilisecondFromByteF(uint32_t byte, const SoundInfo& info)
{
  return static_cast<float>(GetFrameFromByte(byte, info) * 1000.0 / info.rate);
}

uint32_t GetMilisecondFromFrame(uint32_t frame, const SoundInfo& info)
{
  return static_cast<uint32_t>(frame / info.rate);
}


// ---------------------------------- SoundInfo

/* Global-scope default sound info */
SoundInfo g_soundinfo(1, 16, 2, 44100);

SoundInfo::SoundInfo()
  : is_signed(1), bitsize(g_soundinfo.bitsize), channels(g_soundinfo.channels), rate(g_soundinfo.rate)
{}

SoundInfo::SoundInfo(uint8_t signed_, uint8_t bitsize_, uint8_t channels_, uint32_t rate_)
  : is_signed(signed_), bitsize(bitsize_), channels(channels_), rate(rate_) {}

void SoundInfo::SetDefaultSoundInfo(const SoundInfo& info)
{
  g_soundinfo = info;
}

const SoundInfo& SoundInfo::GetDefaultSoundInfo()
{
  return g_soundinfo;
}


// -------------------------------- class Sound

Sound::Sound() : buffer_(nullptr), buffer_size_(0), frame_size_(0),
                 duration_(.0f), is_loading_(false) {}

Sound::Sound(const SoundInfo& info, size_t buffer_size)
  : buffer_(nullptr), buffer_size_(buffer_size), frame_size_(0),
    duration_(.0f), is_loading_(false)
{
  AllocateSize(info, buffer_size);
}

Sound::Sound(const SoundInfo& info, size_t buffer_size, int8_t *p)
  : info_(info), buffer_(p), buffer_size_(buffer_size), frame_size_(0),
    duration_(.0f), is_loading_(false)
{
  frame_size_ = GetFrameFromByte(buffer_size, info);
  duration_ = GetMilisecondFromByteF(buffer_size, info);
}

Sound::~Sound()
{
  Clear();
}

void Sound::set_name(const std::string& name) { name_ = name; }
const std::string& Sound::name() { return name_; }

bool Sound::Load(const std::string& path)
{
  rutil::FileData fd;
  rutil::ReadFileData(path, fd);
  if (fd.IsEmpty())
    return false;
  std::string ext = rutil::GetExtension(path);
  return Load((char*)fd.p, fd.len, ext.c_str());
}

bool Sound::Load(const std::string& path, const SoundInfo& info)
{
  rutil::FileData fd;
  rutil::ReadFileData(path, fd);
  if (fd.IsEmpty())
    return false;
  std::string ext = rutil::GetExtension(path);
  return Load((char*)fd.p, fd.len, ext.c_str(), info);
}

static inline Decoder *CreateDecoder(const char *sig, const char *ext_hint)
{
  if (memcmp("OggS", sig, 4) == 0) return new Decoder_OGG();
  else if (memcmp("RIFF", sig, 4) == 0) return new Decoder_WAV();
  else if (memcmp("fLaC", sig, 4) == 0) return new Decoder_FLAC();
  else if (memcmp("ID3", sig, 4) == 0) return new Decoder_LAME();

  if (ext_hint)
  {
    if (stricmp("OGG", ext_hint) == 0) return new Decoder_OGG();
    else if (stricmp("WAV", ext_hint) == 0) return new Decoder_WAV();
    else if (stricmp("FLAC", ext_hint) == 0) return new Decoder_FLAC();
    else if (stricmp("MP3", ext_hint) == 0) return new Decoder_LAME();
  }

  return nullptr;
}

bool Sound::Load(const char* p, size_t len, const char *ext_hint)
{
  Decoder *decoder = nullptr;
  bool r = false;
  size_t framecount = 0;
  char *buf = 0;

  if (len < 4) return false;
  if (!(decoder = CreateDecoder(p, ext_hint)))
    return false;

  if (decoder->open(p, len))
  {
    r = (framecount = decoder->read(&buf)) != 0;

    if (!r)
    {
      // failure cleanup
      if (buf) free(buf);
    }
    else
    {
      // set buffer
      SetBuffer(decoder->get_info(), framecount, buf);
    }
  }

  delete decoder;
  return true;
}

bool Sound::Load(const char* p, size_t len, const char *ext_hint, const SoundInfo &info)
{
  Decoder *decoder = nullptr;
  bool r = false;
  size_t framecount = 0;
  char *buf = 0;

  if (len < 4) return false;
  if (!(decoder = CreateDecoder(p, ext_hint)))
    return false;

  if (decoder->open(p, len))
  {
    r = (framecount = decoder->readWithFormat(&buf, info)) != 0;

    if (!r)
    {
      // attempt again with general reader
      // if not, failure cleanup
      r = (framecount = decoder->read(&buf)) != 0;
      if (r)
      {
        SetBuffer(decoder->get_info(), framecount, buf);
      }
      else
      {
        if (buf) free(buf);
      }
    }
    else
    {
      // set buffer
      SetBuffer(decoder->get_info(), framecount, buf);
    }
  }

  // do resampling (for channel / rate conversion)
  if (r)
    r = Resample(info);
  else
    Clear();

  delete decoder;
  return r;
}

bool Sound::Save(const std::string& path)
{
  std::map<std::string, std::string> __;
  return Save(path, __, nullptr, 0.6);
}

bool Sound::Save(const std::string& path, const SoundInfo &info)
{
  std::map<std::string, std::string> __;
  return Save(path, __, &info, 0.6);
}

bool Sound::Save(const std::string& path,
  const std::map<std::string, std::string> &metadata,
  const SoundInfo *info,
  double quality)
{
  Encoder *encoder = nullptr;
  std::string ext = rutil::lower(rutil::GetExtension(path));
  bool r = false;

  if (is_empty())
    return false;

  if (ext == "wav")
    encoder = new Encoder_WAV(*this);
  else if (ext == "ogg")
    encoder = new Encoder_OGG(*this);
  else if (ext == "flac")
    encoder = new Encoder_FLAC(*this);

  if (!info)
    info = &get_soundinfo();

  if (!encoder)
    return false;
  else
  {
    for (auto &i : metadata)
      encoder->SetMetadata(i.first, i.second);
    encoder->SetQuality(quality);
    r = encoder->Write(path, *info);
    delete encoder;
    return r;
  }
}

void Sound::Clear()
{
  if (buffer_)
  {
    free(buffer_);
    buffer_ = 0;
    buffer_size_ = 0;
  }
}

bool Sound::Resample(const SoundInfo& info)
{
  // resampling if necessary
  if (!buffer_) return false;
  if (get_soundinfo() != info && !is_empty())
  {
    Sound *new_s = new Sound();
    Sampler sampler(*this, info);
    if (!sampler.Resample(*new_s))
    {
      delete new_s;
      return false;
    }
    if (!new_s->is_empty())
      swap(*new_s);
    delete new_s;
  }
  info_ = info;
  return true;
}

bool Sound::Effect(double pitch, double tempo, double volume)
{
  // resampling for pitch / speed / etc.
  // sound quality is not changed by this method.
  Effector effector;
  effector.SetPitch(pitch);
  effector.SetTempo(tempo);
  effector.SetVolume(volume);
  if (!effector.Resample(*this))
    return false;
  return true;
}

void Sound::AllocateSize(const SoundInfo& info, size_t buffer_size)
{
  AllocateFrame(info, GetFrameFromByte(buffer_size, info));
}

void Sound::AllocateSample(const SoundInfo& info, size_t sample_size)
{
  AllocateFrame(info, sample_size / info.channels);
}

void Sound::AllocateFrame(const SoundInfo& info, size_t frame_size)
{
  Clear();
  info_ = info;
  frame_size_ = frame_size;
  buffer_size_ = GetByteFromFrame(frame_size);
  duration_ = (float)frame_size / info.rate * 1000;
  buffer_ = (int8_t*)calloc(1, buffer_size_);
}

void Sound::AllocateDuration(const SoundInfo& info, uint32_t duration_ms)
{
  AllocateFrame(info, GetFrameFromMilisecond(duration_ms, info));
}

void Sound::SetBuffer(const SoundInfo& info, size_t framecount, void *p)
{
  Clear();
  info_ = info;
  buffer_ = (int8_t*)p;
  buffer_size_ = GetByteFromFrame(framecount);
  frame_size_ = framecount;
  duration_ = (float)framecount / info.rate * 1000;
}

void Sound::SetEmptyBuffer(const SoundInfo& info, size_t framecount)
{
  AllocateSize(info, info.channels * info.bitsize / 8 * framecount);
}

const SoundInfo& Sound::get_soundinfo() const
{
  return info_;
}

size_t Sound::get_total_byte() const
{
  return buffer_size_;
}

size_t Sound::get_frame_count() const
{
  return frame_size_;
}

size_t Sound::get_sample_count() const
{
  return frame_size_ * info_.channels;
}

float Sound::get_duration() const
{
  return duration_;
}

bool Sound::SetSoundFormat(const SoundInfo& info)
{
  return Resample(info);
}

float Sound::GetSoundLevel(size_t offset, size_t sample_len) const
{
  uint64_t levelsum = 0;
  // mixsize : sample count (not frame)
  if (frame_size_ < offset)
    return 0;
  const size_t scansize = std::min(
    (frame_size_ - offset) * info_.channels,
    sample_len);
  const size_t sampleoffset = offset * info_.channels;
  if (scansize == 0) return .0f;
  uint64_t maxval = 0;

  /* sum up samples */
  if (info_.is_signed == 0)
  {
    switch (info_.bitsize)
    {
    case 8:
      maxval = std::numeric_limits<uint8_t>::max();
      for (size_t i = sampleoffset; i < sampleoffset + scansize; ++i)
        levelsum += *((uint8_t*)buffer_ + i);
      break;
    case 16:
      maxval = std::numeric_limits<uint16_t>::max();
      for (size_t i = sampleoffset; i < sampleoffset + scansize; ++i)
        levelsum += *((uint16_t*)buffer_ + i);
      break;
    case 32:
      maxval = std::numeric_limits<uint32_t>::max();
      for (size_t i = sampleoffset; i < sampleoffset + scansize; ++i)
        levelsum += *((uint32_t*)buffer_ + i);
      break;
    default:
      /* 24bit unsigned audio is not supported */
      RMIXER_ASSERT(0);
    }
  }
  else if (info_.is_signed == 1)
  {
    // ignore sign flag
    switch (info_.bitsize)
    {
    case 8:
      maxval = std::numeric_limits<int8_t>::max();
      for (size_t i = sampleoffset; i < sampleoffset + scansize; ++i)
        levelsum += std::abs(*((int8_t*)buffer_ + i));
      break;
    case 16:
      maxval = std::numeric_limits<int16_t>::max();
      for (size_t i = sampleoffset; i < sampleoffset + scansize; ++i)
        levelsum += std::abs(*((int16_t*)buffer_ + i));
      break;
    case 24:
      maxval = 0x007fffff;
      for (size_t i = sampleoffset; i < sampleoffset + scansize; ++i)
        levelsum += *(int32_t*)((int8_t*)buffer_ + i * 3) & 0x007fffff;
      break;
    case 32:
      maxval = std::numeric_limits<int32_t>::max();
      for (size_t i = sampleoffset; i < sampleoffset + scansize; ++i)
        levelsum += std::abs(*((int32_t*)buffer_ + i));
      break;
    default:
      RMIXER_ASSERT(0);
    }
  }
  else if (info_.is_signed == 2)
  {
    constexpr float kRatio = std::numeric_limits<uint32_t>::max() / std::numeric_limits<float>::max();
    switch (info_.bitsize)
    {
    /* XXX: float may be truncated, so treat like unsigned_int_32 */
    case 32:
      maxval = std::numeric_limits<uint32_t>::max();
      for (size_t i = sampleoffset; i < sampleoffset + scansize; ++i)
        levelsum += (uint32_t)std::abs((*((float*)buffer_ + i) * kRatio));
      break;
    default:
      RMIXER_ASSERT(0);
    }
  }
  else
  {
    RMIXER_THROW("Unsupported PCM type.");
  }

  return (levelsum / scansize) / (float)maxval;
}

const int8_t* Sound::get_ptr() const { return buffer_; }
int8_t* Sound::get_ptr() { return buffer_; }

bool Sound::is_empty() const
{
  return buffer_size_ == 0;
}

bool Sound::is_loading() const
{
  return is_loading_;
}

bool Sound::is_loaded() const
{
  return !is_loading_ && !is_empty();
}

size_t Sound::GetByteFromSample(size_t sample_len) const
{
  return rmixer::GetByteFromSample(sample_len, info_);
}

size_t Sound::GetByteFromFrame(size_t frame_len) const
{
  return rmixer::GetByteFromFrame(frame_len, info_);
}

size_t Sound::Mix(int8_t *copy_to, size_t *offset, size_t frame_len) const
{
  if (is_empty()) return 0;
  // mixsize : frame count
  const size_t mixsize = std::min(frame_size_ - *offset, frame_len);
  const size_t smixsize = mixsize * info_.channels;
  const size_t soffset = *offset * info_.channels;
  if (info_.is_signed == 0)
  {
    switch (info_.bitsize)
    {
    case 8:
      pcmmix((uint8_t*)copy_to, (uint8_t*)buffer_ + soffset, smixsize);
      break;
    case 16:
      pcmmix((uint16_t*)copy_to, (uint16_t*)buffer_ + soffset, smixsize);
      break;
    case 32:
      pcmmix((uint32_t*)copy_to, (uint32_t*)buffer_ + soffset, smixsize);
      break;
    default:
      /* 24bit unsigned audio is not supported */
      RMIXER_ASSERT(0);
    }
  }
  else if (info_.is_signed == 1)
  {
    switch (info_.bitsize)
    {
    case 8:
      pcmmix((int8_t*)copy_to, (int8_t*)buffer_ + soffset, smixsize);
      break;
    case 16:
      pcmmix((int16_t*)copy_to, (int16_t*)buffer_ + soffset, smixsize);
      break;
    case 24:
      pcmmix24((int8_t*)copy_to, (int8_t*)buffer_ + soffset * 3, smixsize);
      break;
    case 32:
      pcmmix((int32_t*)copy_to, (int32_t*)buffer_ + soffset, smixsize);
      break;
    default:
      RMIXER_ASSERT(0);
    }
  }
  else if (info_.is_signed == 2)
  {
    switch (info_.bitsize)
    {
    case 32:
      pcmmix((float*)copy_to, (float*)buffer_ + soffset, smixsize);
      break;
    default:
      RMIXER_ASSERT(0);
    }
  }
  else
  {
    RMIXER_THROW("Unsupported PCM type.");
  }
  *offset += mixsize;
  return mixsize;
}

/* @brief same as Mix method, but with volume */
size_t Sound::MixWithVolume(int8_t *copy_to, size_t *offset, size_t frame_len, float volume) const
{
  if (is_empty()) return 0;
  const size_t mixsize = std::min(frame_size_ - *offset, frame_len);
  const size_t smixsize = mixsize * info_.channels;
  const size_t soffset = *offset * info_.channels;
  if (info_.is_signed == 0)
  {
    switch (info_.bitsize)
    {
    case 8:
      pcmmix((uint8_t*)copy_to, (uint8_t*)buffer_ + soffset, smixsize, volume);
      break;
    case 16:
      pcmmix((uint16_t*)copy_to, (uint16_t*)buffer_ + soffset, smixsize, volume);
      break;
    case 32:
      pcmmix((uint32_t*)copy_to, (uint32_t*)buffer_ + soffset, smixsize, volume);
      break;
    default:
      /* 24bit unsigned audio is not supported */
      RMIXER_ASSERT(0);
    }
  }
  else if (info_.is_signed == 1)
  {
    switch (info_.bitsize)
    {
    case 8:
      pcmmix((int8_t*)copy_to, (int8_t*)buffer_ + soffset, smixsize, volume);
      break;
    case 16:
      pcmmix((int16_t*)copy_to, (int16_t*)buffer_ + soffset, smixsize, volume);
      break;
    case 24:
      pcmmix24((int8_t*)copy_to, (int8_t*)buffer_ + soffset * 3, smixsize, volume);
      break;
    case 32:
      pcmmix((int32_t*)copy_to, (int32_t*)buffer_ + soffset, smixsize, volume);
      break;
    default:
      RMIXER_ASSERT(0);
    }
  }
  else if (info_.is_signed == 2)
  {
    switch (info_.bitsize)
    {
    case 32:
      pcmmix((float*)copy_to, (float*)buffer_ + soffset, smixsize, volume);
      break;
    default:
      RMIXER_ASSERT(0);
    }
  }
  else
  {
    RMIXER_THROW("Unsupported PCM type.");
  }
  *offset += mixsize;
  return mixsize;
}

size_t Sound::Copy(int8_t *p, size_t *offset, size_t frame_len) const
{
  if (is_empty()) return 0;
  const size_t mixsize = std::min(frame_size_ - *offset, frame_len);
  const size_t smixsize = mixsize * info_.channels;
  const size_t soffset = *offset * info_.channels;
  if (info_.is_signed == 0)
  {
    switch (info_.bitsize)
    {
    case 8:
      pcmcpy((uint8_t*)p, (uint8_t*)buffer_ + soffset, smixsize);
      break;
    case 16:
      pcmcpy((uint16_t*)p, (uint16_t*)buffer_ + soffset, smixsize);
      break;
    case 32:
      pcmcpy((uint32_t*)p, (uint32_t*)buffer_ + soffset, smixsize);
      break;
    default:
      /* 24bit unsigned audio is not supported */
      RMIXER_ASSERT(0);
    }
  }
  else if (info_.is_signed == 1)
  {
    switch (info_.bitsize)
    {
    case 8:
      pcmcpy((int8_t*)p, (int8_t*)buffer_ + soffset, smixsize);
      break;
    case 16:
      pcmcpy((int16_t*)p, (int16_t*)buffer_ + soffset, smixsize);
      break;
    case 24:
      pcmcpy24((int8_t*)p, (int8_t*)buffer_ + soffset * 3, smixsize);
      break;
    case 32:
      pcmcpy((int32_t*)p, (int32_t*)buffer_ + soffset, smixsize);
      break;
    default:
      RMIXER_ASSERT(0);
    }
  }
  else if (info_.is_signed == 2)
  {
    switch (info_.bitsize)
    {
    case 32:
      pcmcpy((float*)p, (float*)buffer_ + soffset, smixsize);
      break;
    default:
      RMIXER_ASSERT(0);
    }
  }
  else
  {
    RMIXER_THROW("Unsupported PCM type.");
  }
  *offset += mixsize;
  return mixsize;
}

size_t Sound::CopyWithVolume(int8_t *p, size_t *offset, size_t frame_len, float volume) const
{
  if (is_empty()) return 0;
  const size_t mixsize = std::min(frame_size_ - *offset, frame_len);
  const size_t smixsize = mixsize * info_.channels;
  const size_t soffset = *offset * info_.channels;
  if (info_.is_signed == 0)
  {
    switch (info_.bitsize)
    {
    case 8:
      pcmcpy((uint8_t*)p, (uint8_t*)buffer_ + soffset, smixsize, volume);
      break;
    case 16:
      pcmcpy((uint16_t*)p, (uint16_t*)buffer_ + soffset, smixsize, volume);
      break;
    case 32:
      pcmcpy((uint32_t*)p, (uint32_t*)buffer_ + soffset, smixsize, volume);
      break;
    default:
      /* 24bit unsigned audio is not supported */
      RMIXER_ASSERT(0);
    }
  }
  else if (info_.is_signed == 1)
  {
    switch (info_.bitsize)
    {
    case 8:
      pcmcpy((int8_t*)p, (int8_t*)buffer_ + soffset, smixsize, volume);
      break;
    case 16:
      pcmcpy((int16_t*)p, (int16_t*)buffer_ + soffset, smixsize, volume);
      break;
    case 24:
      pcmcpy24((int8_t*)p, (int8_t*)buffer_ + soffset * 3, smixsize, volume);
      break;
    case 32:
      pcmcpy((int32_t*)p, (int32_t*)buffer_ + soffset, smixsize, volume);
      break;
    default:
      RMIXER_ASSERT(0);
    }
  }
  else if (info_.is_signed == 2)
  {
    switch (info_.bitsize)
    {
    case 32:
      pcmcpy((float*)p, (float*)buffer_ + soffset, smixsize, volume);
      break;
    default:
      RMIXER_ASSERT(0);
    }
  }
  else
  {
    RMIXER_THROW("Unsupported PCM type.");
  }
  *offset += mixsize;
  return mixsize;
}

void Sound::swap(Sound &s)
{
  std::swap(name_, s.name_);
  std::swap(info_, s.info_);
  std::swap(buffer_, s.buffer_);
  std::swap(duration_, s.duration_);
  std::swap(is_loading_, s.is_loading_);    // XXX: is it okay?
  std::swap(buffer_size_, s.buffer_size_);
  std::swap(frame_size_, s.frame_size_);
}

void Sound::copy(const Sound &src)
{
  Clear();
  name_ = src.name_;
  info_ = src.info_;
  buffer_size_ = src.buffer_size_;
  frame_size_ = src.frame_size_;
  duration_ = src.duration_;
  buffer_ = (int8_t*)malloc(buffer_size_);
  memcpy(buffer_, src.buffer_, buffer_size_);
  is_loading_ = false;
}

Sound* Sound::clone() const
{
  Sound *s = new Sound();
  s->buffer_ = (int8_t*)malloc(buffer_size_);
  memcpy(s->buffer_, buffer_, buffer_size_);
  s->info_ = info_;
  s->is_loading_ = false;   // always false for duplicated object
  s->buffer_size_ = buffer_size_;
  s->frame_size_ = frame_size_;
  s->duration_ = duration_;
  return s;
}

std::string Sound::toString() const
{
  if (!buffer_)
    return "empty";
  else
  {
    char tmp[256];
    const SoundInfo &s = get_soundinfo();
    const char *typestr = "Unknown";
    char *p;
    if (s.is_signed == 0)
    {
      switch (s.bitsize)
      {
        /* XXX: should support 2bit audio? */
      case 4:
        typestr = "U4";
        break;
      case 8:
        typestr = "U8";
        break;
      case 16:
        typestr = "U16";
        break;
      case 24:
        typestr = "U24";
        break;
      case 32:
        typestr = "U32";
        break;
      default:
        break;
      }
    }
    else if (s.is_signed == 1)
    {
      switch (s.bitsize)
      {
      case 4:
        typestr = "S4";
        break;
      case 8:
        typestr = "S8";
        break;
      case 16:
        typestr = "S16";
        break;
      case 24:
        typestr = "S24";
        break;
      case 32:
        typestr = "S32";
        break;
      default:
        break;
      }
    }
    else
    {
      switch (s.bitsize)
      {
      case 32:
        typestr = "F32";
        break;
      default:
        break;
      }
    }
    p = tmp + sprintf(tmp, "PCMSound %dHz / %dCh / %dBit (%s), Frame %u, Size %u",
      s.rate, s.channels, s.bitsize, typestr, frame_size_, buffer_size_);
    if (enable_detailed_log)
    {
      if (frame_size_ > 128)
      {
        // sampling data goes here
        float l = GetSoundLevel(frame_size_ / 2, 128);
        char d[4];
        memcpy(d, buffer_ + buffer_size_ / 2, 4);
        sprintf(p, "\nMiddle Lvl%.2f, Hex %02X %02X %02X %02X",
          l, d[0], d[1], d[2], d[3]);
      }
      else
      {
        sprintf(p, "\n(Audio is too small)");
      }
    }
    return tmp;
  }
}

void Sound::EnableDetailedLog(bool v)
{
  enable_detailed_log = v;
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
