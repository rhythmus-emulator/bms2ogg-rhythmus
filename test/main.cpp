#include <iostream>
#include <gtest/gtest.h>
#include "Mixer.h"
#include "SoundPool.h"
#include "Sampler.h"
#include "rparser.h"

#define TEST_PATH std::string("../test/test/")

using namespace rmixer;

static inline void print_data_in_hex(const uint8_t* c, size_t len, std::ostream &os)
{
  const size_t a = true ? 'A' - 1 : 'a' - 1;
  for (size_t i = 0; i < len; ++i)
  {
    os << (char)(*c > 0x9F ? (*c / 16 - 9) | a : *c / 16 | '0')
       << (char)((*c & 0xF) > 9 ? (*c % 16 - 9) | a : *c % 16 | '0') << " ";
    ++c;
  }
}

static std::string get_data_in_hex(const uint8_t* c, size_t len)
{
  std::stringstream ss;
  print_data_in_hex(c, len, ss);
  return ss.str();
}

/* @brief all sounds are 1024 frame size */
class TestPCMData
{
public:
  TestPCMData();

  Sound s8_null;
  Sound s16_null;
  Sound s32_null;
  Sound u8_null;
  Sound u16_null;
  Sound u32_null;

  Sound s8_FF;
  Sound s16_FF;
  Sound s32_FF;
  Sound s8_7F;
  Sound s16_7F;
  Sound s32_7F;
  Sound u8_FF;
  Sound u16_FF;
  Sound u32_FF;

  Sound s8_8000hz_7F;
  Sound s16_8000hz_7F;
  Sound s32_8000hz_7F;
  Sound u8_8000hz_FF;
  Sound u16_8000hz_FF;
  Sound u32_8000hz_FF;

  Sound s8_1ch_80;
  Sound s16_1ch_80;
  Sound s32_1ch_80;
  Sound u8_1ch_FF;
  Sound u16_1ch_FF;
  Sound u32_1ch_FF;
};

constexpr size_t kPCMFrameSize = 1024;

static inline void FillPCMData(Sound &s, char b, unsigned is_signed,
                               unsigned rate, unsigned channel, unsigned bitsize)
{
  SoundInfo info(is_signed, bitsize, channel, rate);
  s.AllocateFrame(info, kPCMFrameSize);
  memset(s.get_ptr(), b, s.get_total_byte());
}

TestPCMData::TestPCMData()
{
  FillPCMData(s8_null, 0, 1, 44100, 2, 8);
  FillPCMData(s16_null, 0, 1, 44100, 2, 16);
  FillPCMData(s32_null, 0, 1, 44100, 2, 32);
  FillPCMData(u8_null, 0, 0, 44100, 2, 8);
  FillPCMData(u16_null, 0, 0, 44100, 2, 16);
  FillPCMData(u32_null, 0, 0, 44100, 2, 32);
  FillPCMData(s8_FF, '\xFF', 1, 44100, 2, 8);
  FillPCMData(s16_FF, '\xFF', 1, 44100, 2, 16);
  FillPCMData(s32_FF, '\xFF', 1, 44100, 2, 32);
  FillPCMData(s8_7F, '\x7F', 1, 44100, 2, 8);
  FillPCMData(s16_7F, '\x7F', 1, 44100, 2, 16);
  FillPCMData(s32_7F, '\x7F', 1, 44100, 2, 32);
  FillPCMData(u8_FF, '\xFF', 0, 44100, 2, 8);
  FillPCMData(u16_FF, '\xFF', 0, 44100, 2, 16);
  FillPCMData(u32_FF, '\xFF', 0, 44100, 2, 32);
  FillPCMData(s8_8000hz_7F, '\x7F', 1, 8000, 2, 8);
  FillPCMData(s16_8000hz_7F, '\x7F', 1, 8000, 2, 16);
  FillPCMData(s32_8000hz_7F, '\x7F', 1, 8000, 2, 32);
  FillPCMData(u8_8000hz_FF, '\xFF', 0, 8000, 2, 8);
  FillPCMData(u16_8000hz_FF, '\xFF', 0, 8000, 2, 16);
  FillPCMData(u32_8000hz_FF, '\xFF', 0, 8000, 2, 32);
  FillPCMData(s8_1ch_80, '\x80', 1, 44100, 1, 8);
  FillPCMData(s16_1ch_80, '\x80', 1, 44100, 1, 16);
  FillPCMData(s32_1ch_80, '\x80', 1, 44100, 1, 32);
  FillPCMData(u8_1ch_FF, '\xFF', 0, 44100, 1, 8);
  FillPCMData(u16_1ch_FF, '\xFF', 0, 44100, 1, 16);
  FillPCMData(u32_1ch_FF, '\xFF', 0, 44100, 1, 32);
}

static const TestPCMData gTestPCMData;

// ---- Test Begin ----

TEST(BASIC, PCMDATA)
{
  // frame size check
  EXPECT_EQ(kPCMFrameSize, gTestPCMData.s16_1ch_80.get_frame_count());
  EXPECT_EQ(kPCMFrameSize, gTestPCMData.s16_7F.get_frame_count());

  // byte size check
  EXPECT_EQ(kPCMFrameSize * 2 * 16 / 8, gTestPCMData.s16_null.get_total_byte());
  EXPECT_EQ(kPCMFrameSize * 2 * 8 / 8, gTestPCMData.s8_8000hz_7F.get_total_byte());
  EXPECT_EQ(kPCMFrameSize * 1 * 16 / 8, gTestPCMData.u16_1ch_FF.get_total_byte());

  // duration check
  EXPECT_EQ(gTestPCMData.s16_1ch_80.get_duration(), gTestPCMData.u8_FF.get_duration());
  EXPECT_NEAR(8000 / 44100.0f,
    gTestPCMData.u8_FF.get_duration() / gTestPCMData.u8_8000hz_FF.get_duration(),
    0.01f);
}

TEST(BASIC, LEVEL)
{
  // 1. signed, 00
  EXPECT_NEAR(0.0, gTestPCMData.s32_null.GetSoundLevel(128, 128), 0.01);

  // 2. unsigned, 00
  EXPECT_NEAR(0.0, gTestPCMData.u16_null.GetSoundLevel(128, 128), 0.01);

  // 3. unsigned 0xFF (256)
  EXPECT_NEAR(1.0, gTestPCMData.u8_8000hz_FF.GetSoundLevel(128, 128), 0.01);

  // 3. signed, 0x7F (127)
  EXPECT_NEAR(1.0, gTestPCMData.s8_8000hz_7F.GetSoundLevel(128, 128), 0.01);
  EXPECT_NEAR(1.0, gTestPCMData.s16_8000hz_7F.GetSoundLevel(128, 128), 0.01);
  EXPECT_NEAR(1.0, gTestPCMData.s32_8000hz_7F.GetSoundLevel(128, 128), 0.01);

  // 4. signed, 0xFF (-1)
  EXPECT_NEAR(0.008, gTestPCMData.s8_FF.GetSoundLevel(128, 128), 0.001);
  EXPECT_NEAR(0, gTestPCMData.s16_FF.GetSoundLevel(128, 128), 0.001);
  EXPECT_NEAR(0, gTestPCMData.s32_FF.GetSoundLevel(128, 128), 0.001);

  // 5. signed, 0x80 (-128)
  EXPECT_NEAR(1.0, gTestPCMData.s16_1ch_80.GetSoundLevel(128, 128), 0.01);
}

// TODO: sampler test (rate, channel conversion)
TEST(BASIC, RESAMPLER)
{
  // 1. 0xFF - U8,8000,2ch to S16,44100,2ch
  // should be high level.
  {
    SoundInfo info(1, 16, 2, 44100);
    Sound s16_2ch_7F;
    Resample(s16_2ch_7F, gTestPCMData.u8_8000hz_FF, info);
    EXPECT_EQ(2, s16_2ch_7F.get_soundinfo().channels);
    EXPECT_EQ(44100, s16_2ch_7F.get_soundinfo().rate);
    EXPECT_EQ(16, s16_2ch_7F.get_soundinfo().bitsize);
    EXPECT_NEAR(s16_2ch_7F.get_duration(), gTestPCMData.u8_8000hz_FF.get_duration(), 0.1);
    EXPECT_NEAR(1.0, s16_2ch_7F.GetSoundLevel(128, 128), 0.01);
    EXPECT_NEAR(44100 / 8000.0f,
      s16_2ch_7F.get_frame_count() / (float)gTestPCMData.u8_8000hz_FF.get_frame_count(),
      0.01f);
  }

  // 2. 0xFF - S32,44100,1ch to U16,8000,2ch
  // should be low level.
  {
    SoundInfo info(0, 16, 2, 8000);
    Sound u16_2ch_01;
    Resample(u16_2ch_01, gTestPCMData.s32_1ch_80, info);
    EXPECT_EQ(2, u16_2ch_01.get_soundinfo().channels);
    EXPECT_EQ(8000, u16_2ch_01.get_soundinfo().rate);
    EXPECT_EQ(16, u16_2ch_01.get_soundinfo().bitsize);
    EXPECT_NEAR(u16_2ch_01.get_duration(), gTestPCMData.s32_1ch_80.get_duration(), 0.1);
    EXPECT_NEAR(0.0, u16_2ch_01.GetSoundLevel(128, 128), 0.01);
    EXPECT_NEAR(8000 / 44100.f,
      u16_2ch_01.get_frame_count() / (float)gTestPCMData.s32_1ch_80.get_frame_count(),
      0.01f);
  }
}

TEST(BASIC, MIX)
{
  // 1. signed and positive signal (should be clipped)
  Sound s16_7F;
  s16_7F.copy(gTestPCMData.s16_7F);
  EXPECT_EQ((int16_t)0x7F7F, *(int16_t*)s16_7F.get_ptr());
  pcmmix((int16_t*)s16_7F.get_ptr(), (int16_t*)gTestPCMData.s16_7F.get_ptr(), s16_7F.get_sample_count());
  EXPECT_EQ((int16_t)0x7FFF, *(int16_t*)s16_7F.get_ptr());

  // 2. signed and negative signal
  Sound s8_1ch_80;
  s8_1ch_80.copy(gTestPCMData.s8_1ch_80);
  EXPECT_EQ((int16_t)0x8080, *(int16_t*)s8_1ch_80.get_ptr());
  pcmmix(s8_1ch_80.get_ptr(), gTestPCMData.s8_1ch_80.get_ptr(), s8_1ch_80.get_sample_count());
  EXPECT_EQ((int16_t)0x8080, *(int16_t*)s8_1ch_80.get_ptr());

  // 3. unsigned, zero + 0xFF
  Sound u32_null;
  u32_null.copy(gTestPCMData.u32_null);
  EXPECT_EQ(0x0000u, *(uint16_t*)u32_null.get_ptr());
  pcmmix((int32_t*)u32_null.get_ptr(), (int32_t*)gTestPCMData.u32_FF.get_ptr(), u32_null.get_sample_count());
  EXPECT_EQ(0xFFFFu, *(uint16_t*)u32_null.get_ptr());

  // 4. unsigned, 0xFF + 0xFF (should be clipped)
  Sound u32_FF;
  u32_FF.copy(gTestPCMData.u32_FF);
  EXPECT_EQ(0xFFFFu, *(uint16_t*)u32_FF.get_ptr());
  pcmmix((uint32_t*)u32_FF.get_ptr(), (uint32_t*)gTestPCMData.u32_FF.get_ptr(), u32_FF.get_sample_count());
  EXPECT_EQ(0xFFFFu, *(uint16_t*)u32_FF.get_ptr());

  // XXX: F16, F32 is not supported, so not tested now
}

// TODO: soundeffector test
TEST(BASIC, SOUNDEFFECTOR)
{
  // 1. U16 0xFF with volume 0.5
  {
    Sound u16_FF;
    u16_FF.copy(gTestPCMData.u16_FF);
    EXPECT_EQ(0xFFFFu, *(uint16_t*)u16_FF.get_ptr());
    u16_FF.Effect(1.0, 1.0, 0.5);
    EXPECT_FALSE(u16_FF.is_empty());
    EXPECT_EQ(kPCMFrameSize, u16_FF.get_frame_count());
    EXPECT_NEAR(0.5, u16_FF.GetSoundLevel(128, 128), 0.05);
  }

  // 2. S16 0x7F with volume 0.5
  {
    Sound s16_7F;
    s16_7F.copy(gTestPCMData.s16_7F);
    EXPECT_EQ(0x7F7Fu, *(uint16_t*)s16_7F.get_ptr());
    s16_7F.Effect(1.0, 1.0, 0.5);
    EXPECT_FALSE(s16_7F.is_empty());
    EXPECT_EQ(kPCMFrameSize, s16_7F.get_frame_count());
    EXPECT_NEAR(0.5, s16_7F.GetSoundLevel(128, 128), 0.05);
  }

  // 3. S16 0x80(=~ -32500) with volume 0.5 + pitch 2x
  {
    Sound s16_1ch_80;
    s16_1ch_80.copy(gTestPCMData.s16_1ch_80);
    EXPECT_EQ(0x8080u, *(uint16_t*)s16_1ch_80.get_ptr());
    s16_1ch_80.Effect(2.0, 1.0, 0.5);
    EXPECT_FALSE(s16_1ch_80.is_empty());
    EXPECT_EQ(kPCMFrameSize / 2, s16_1ch_80.get_frame_count());
    EXPECT_NEAR(0.5, s16_1ch_80.GetSoundLevel(128, 128), 0.05);
  }

  // 4. S32 0xFF(== -1) with volume 0.5 + pitch 0.5x
  {
    Sound s32_FF;
    s32_FF.copy(gTestPCMData.s32_FF);
    EXPECT_EQ(0xFFFFu, *(uint16_t*)s32_FF.get_ptr());
    s32_FF.Effect(0.5, 1.0, 0.5);
    EXPECT_FALSE(s32_FF.is_empty());
    EXPECT_EQ(kPCMFrameSize * 2, s32_FF.get_frame_count());
    EXPECT_NEAR(0.0, s32_FF.GetSoundLevel(128, 128), 0.05);
  }

  // 5. 0x80(~= -32500) with pitch 2x + tempo(length) 2x
  {
    Sound s16_1ch_80;
    s16_1ch_80.copy(gTestPCMData.s16_1ch_80);
    EXPECT_EQ(0x8080u, *(uint16_t*)s16_1ch_80.get_ptr());
    s16_1ch_80.Effect(2.0, 2.0, 1.0);
    EXPECT_FALSE(s16_1ch_80.is_empty());
    EXPECT_EQ(kPCMFrameSize, s16_1ch_80.get_frame_count());
    EXPECT_NEAR(0.95, s16_1ch_80.GetSoundLevel(128, 128), 0.05);
  }

  // 6. 0x80(~= -32500) with pitch 0.5x + tempo(length) 0.5x
  {
    Sound s16_1ch_80;
    s16_1ch_80.copy(gTestPCMData.s16_1ch_80);
    EXPECT_EQ(0x8080u, *(uint16_t*)s16_1ch_80.get_ptr());
    s16_1ch_80.Effect(0.5, 0.5, 1.0);
    EXPECT_FALSE(s16_1ch_80.is_empty());
    EXPECT_EQ(kPCMFrameSize, s16_1ch_80.get_frame_count());
    EXPECT_NEAR(0.95, s16_1ch_80.GetSoundLevel(128, 128), 0.05);
  }
}

TEST(DECODER, WAV)
{
  using namespace rmixer;
  const auto wav_files = {
    "1-Loop-1-16.wav",
    "1-loop-2-02.wav",
    "8k8bitpcm.wav",
    "8kadpcm.wav",
    //"8kmp38.wav"  // this is MPEG-3 layer codec; too much!
  };

  // Open as various input format
  for (const auto& wav_fn : wav_files)
  {
    Sound s;
    std::cout << "Open sound file: " << wav_fn << " ";
    EXPECT_TRUE(s.Load(TEST_PATH + wav_fn));
    EXPECT_FALSE(s.is_empty());
    std::cout << s.toString() << std::endl;
  }

  // Open with given format
  {
    Sound s16_audio, s32_audio;
    SoundInfo s16_sinfo(1, 16, 2, 44100);
    SoundInfo s32_sinfo(1, 32, 2, 44100);
    const char *fn = *(wav_files.begin() + 2);
    s16_audio.Load(TEST_PATH + fn, s16_sinfo);
    s32_audio.Load(TEST_PATH + fn, s32_sinfo);
    EXPECT_EQ("PCMSound 44100Hz / 2Ch / 16Bit (S16), Frame 110488, Size 441952",
      s16_audio.toString());
    EXPECT_EQ("PCMSound 44100Hz / 2Ch / 32Bit (S32), Frame 110488, Size 883904",
      s32_audio.toString());
  }
}

TEST(ENCODER, WAV)
{
  using namespace rmixer;
  auto wav_files = {
    "1-Loop-1-16.wav",
    "1-loop-2-02.wav",
    "8kadpcm.wav",  // 4bit adpcm wav test
  };
  Sound s[3], out;
  SoundInfo target_quality(1, 16, 2, 44100);
  int i = 0;

  for (auto& wav_fn : wav_files)
  {
    std::cout << "Open sound file: " << wav_fn << " ";
    s[i].Resample(target_quality);
    s[i].Load(TEST_PATH + wav_fn);
    std::cout << s[i].toString() << std::endl;
    i++;
  }

  EXPECT_TRUE(s[2].Save(TEST_PATH + "test_wav.wav"));
  EXPECT_TRUE(s[2].Save(TEST_PATH + "test_wav_S16.wav", SoundInfo(1, 16, 2, 8000)));
  EXPECT_TRUE(s[2].Save(TEST_PATH + "test_wav_F32.wav", SoundInfo(2, 32, 2, 44100)));
}

TEST(DECODER, OGG)
{
  using namespace rmixer;
  auto wav_files = {
    "m09.ogg",
  };
  Sound s;

  for (auto& wav_fn : wav_files)
  {
    std::cout << "Open sound file: " << wav_fn << " ";
    ASSERT_TRUE(s.Load(TEST_PATH + wav_fn));
    std::cout << s.toString() << std::endl;
  }
  
  // just for test listening
  EXPECT_TRUE(s.Save(TEST_PATH + "test_ogg.wav"));
  EXPECT_TRUE(s.Save(TEST_PATH + "test_ogg_S16.wav", SoundInfo(1, 16, 2, 44100)));
}

TEST(ENCODER, OGG)
{
  using namespace rmixer;
  auto wav_files = {
    "1-Loop-1-16.wav",
    "1-loop-2-02.wav",
  };
  SoundInfo target_quality(1, 16, 2, 44100);
  Sound s[2];
  int i;

  // load
  i = 0;
  for (auto& wav_fn : wav_files)
  {
    s[i++].Load(TEST_PATH + wav_fn);
  }

  EXPECT_TRUE(s[1].Save(TEST_PATH + "test_ogg.ogg"));
}

TEST(DECODER, MP3)
{
  using namespace rmixer;
  auto wav_files = {
    "gtr-jazz.mp3",
  };
  Sound s;

  for (auto& wav_fn : wav_files)
  {
    std::cout << "Open sound file: " << wav_fn << " ";
    ASSERT_TRUE(s.Load(TEST_PATH + wav_fn));
    std::cout << s.toString() << std::endl;
  }

  // just for test listing
  EXPECT_TRUE(s.Save(TEST_PATH + "test_mp3.wav"));
}

TEST(DECODER, FLAC)
{
  using namespace rmixer;
  auto wav_files = {
    "sample.flac",
  };
  Sound s;

  for (auto& wav_fn : wav_files)
  {
    std::cout << "Open sound file: " << wav_fn << " ";
    ASSERT_TRUE(s.Load(TEST_PATH + wav_fn));
    std::cout << s.toString() << std::endl;
  }

  // just for test listing
  EXPECT_TRUE(s.Save(TEST_PATH + "test_flac.wav"));
}

TEST(ENCODER, FLAC)
{
  using namespace rmixer;
  auto wav_files = {
    "test_out.ogg",
  };
  Sound s;

  for (auto& wav_fn : wav_files)
  {
    std::cout << "Open sound file: " << wav_fn << " ";
    ASSERT_TRUE(s.Load(TEST_PATH + wav_fn));
    std::cout << s.toString() << std::endl;
  }

  EXPECT_TRUE(s.Save(TEST_PATH + "test_flac_enc.flac"));
}

TEST(MIXER, SIMPLE)
{
  // manual mixing ...
  using namespace rmixer;
  auto wav_files = {
    "1-Loop-1-16.wav",
    "1-loop-2-02.wav",
    "8kadpcm.wav",  // 4bit adpcm wav test
  };
  SoundInfo target_quality(1, 16, 2, 44100);
  Mixer mixer(target_quality, 16);
  Sound *s[3];
  Channel *ch[3];
  Sound out;
  int i = 0;

  // load files
  for (auto& wav_fn : wav_files)
  {
    std::cout << "Open sound file: " << wav_fn << " ";
    s[i] = mixer.CreateSound((TEST_PATH + wav_fn).c_str(), false);
    ch[i] = mixer.PlaySound(s[i], false);
    std::cout << s[i]->toString() << std::endl;
    i++;
  }

#if 0
  EXPECT_STREQ("90 03 90 03 19 FE 19 FE ",
    get_data_in_hex((uint8_t*)s[0].get_buffer()->get_ptr(), 8).c_str());

  // just checking is_memory_valid? at last section of buffer
  print_data_in_hex((uint8_t*)(s_resample[0].ptr() + s_resample[0].buffer_byte_size() - 8), 8, std::cout);
  std::cout << std::endl;
#endif

  // start mixing
  out.AllocateDuration(target_quality, 5000);
  char *p = (char*)out.get_ptr();

  // 0 sec
  mixer.MixAll(p, 44100);   p += GetByteFromFrame(44100, target_quality);

  // 1 sec
  ch[0]->Play();
  mixer.MixAll(p, 44100);   p += GetByteFromFrame(44100, target_quality);

  // 2 sec
  ch[0]->Play();
  ch[1]->Play();
  mixer.MixAll(p, 44100);   p += GetByteFromFrame(44100, target_quality);

  // 3 sec
  ch[1]->Play();
  ch[2]->Play();
  mixer.MixAll(p, 44100);   p += GetByteFromFrame(44100, target_quality);

  // 4 sec
  mixer.MixAll(p, 44100);   p += GetByteFromFrame(44100, target_quality);

  // 5 sec
  mixer.MixAll(p, 44100);   p += GetByteFromFrame(44100, target_quality);

#if 0
  EXPECT_STREQ("90 03 90 03 19 FE 19 FE ",
    get_data_in_hex((uint8_t*)out.get_buffer()->get_ptr(), 8).c_str());
#endif

  EXPECT_TRUE(out.Save(TEST_PATH + "test_mixer_simple.wav"));
}

TEST(MIXER, BMS)
{
  // test for seamless real-time sound encoding
  using namespace rmixer;

  /* prepare song & chart */
  rparser::Song song;
  ASSERT_TRUE(song.Open(TEST_PATH + u8"ìÑ¡¡ãó¡¡ÞÀ¡¡Íº¡¡ªÇ¡¡ïÎ¡¡ò­.zip"));
  rparser::Directory *songresource = song.GetDirectory();
  rparser::Chart *c = song.GetChart(0);
  ASSERT_TRUE(c);
  c->Update();

  /* prepare SoundPool & mixer */
  const size_t channel_count = 2048;
  SoundInfo mixinfo(1, 16, 2, 44100);
  Mixer mixer(mixinfo, channel_count);
  KeySoundPoolWithTime soundpool(&mixer, channel_count);

  /* load resource & lane-channel mapping table */
  soundpool.LoadFromChartAndSound(*c);

  /* misc setting: set volume */
  soundpool.SetVolume(0.8f);

  /* save with metadata */
  std::map<std::string, std::string> metadata;
  Sound s;
  auto &md = c->GetMetaData();
  metadata["TITLE"] = md.title;
  metadata["ARTIST"] = md.artist;
  soundpool.SetAutoPlay(true);
  soundpool.RecordToSound(s);
  EXPECT_TRUE(s.Save(
    TEST_PATH + "test_bms.ogg",
    metadata,
    nullptr,
    0.6
  ));

  /* Cleanup */
  song.Close();
}

TEST(MIXER, MIDI)
{
  /** midi mixing test with VOS file. */

  // prepare mixing
  using namespace rmixer;
  SoundInfo sinfo(1, 16, 2, 44100);
  Mixer mixer(sinfo, 1024);
  mixer.InitializeMidi((TEST_PATH + "midi.cfg").c_str());
  KeySoundPoolWithTime soundpool(&mixer, 128);

  // open song
  rparser::Song song;
  ASSERT_TRUE(song.Open(TEST_PATH + u8"1.vos"));
  rparser::Chart *c = song.GetChart(0);
  ASSERT_TRUE(c);
  c->Update();

  // load chart into soundpool
  soundpool.LoadFromChartAndSound(*c);

  // mixing & save
  Sound s;
  soundpool.RecordToSound(s);
  EXPECT_TRUE(s.Save(TEST_PATH + "test_midi.ogg"));

  // cleanup
  song.Close();
}

TEST(SAMPLER, PITCH)
{
  // MUST precede MIXER.BMS test

  using namespace rmixer;
  Sound s;

  ASSERT_TRUE(s.Load(TEST_PATH + "test_bms.ogg"));
  EXPECT_TRUE(s.Effect(1.5, 1.0, 1.0));
  ASSERT_TRUE(s.Save(TEST_PATH + "test_bms_resample1.ogg"));
}

TEST(SAMPLER, TEMPO)
{
  // PITCH & TEMPO move, which results in PITCH shifting without changing duration.
  // MUST precede BMS_ENCODING_ZIP test

  using namespace rmixer;
  Sound s;

  ASSERT_TRUE(s.Load(TEST_PATH + "test_bms.ogg"));
  EXPECT_TRUE(s.Effect(1.0, 0.666, 1.0));
  ASSERT_TRUE(s.Save(TEST_PATH + "test_bms_resample2.ogg"));
}

int main(int argc, char **argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  //::testing::FLAGS_gtest_filter = "MIXER.*";
  ::testing::FLAGS_gtest_catch_exceptions = 0;  // ASSERT to DEBUGGER
  return RUN_ALL_TESTS();
}
