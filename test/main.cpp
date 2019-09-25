#include <iostream>
#include <gtest/gtest.h>
#include "Mixer.h"
#include "SoundPool.h"
#include "rparser.h"

#define TEST_PATH std::string("../test/test/")

inline void print_sound_info(const rmixer::SoundInfo &info)
{
  std::cout << "(ch" << (int)info.channels << ", bps" << info.bitsize << ", rate" << info.rate << ")";
}

inline void print_data_in_hex(const uint8_t* c, size_t len, std::ostream &os)
{
  const size_t a = true ? 'A' - 1 : 'a' - 1;
  for (size_t i = 0; i < len; ++i)
  {
    os << (char)(*c > 0x9F ? (*c / 16 - 9) | a : *c / 16 | '0')
       << (char)((*c & 0xF) > 9 ? (*c % 16 - 9) | a : *c % 16 | '0') << " ";
    ++c;
  }
}

std::string get_data_in_hex(const uint8_t* c, size_t len)
{
  std::stringstream ss;
  print_data_in_hex(c, len, ss);
  return ss.str();
}

TEST(DECODER, WAV)
{
  using namespace rmixer;
  auto wav_files = {
    "1-Loop-1-16.wav",
    "1-loop-2-02.wav",
    "8k8bitpcm.wav",
    "8kadpcm.wav",
    //"8kmp38.wav"  // this is MPEG-3 layer codec; too much!
  };
  for (auto& wav_fn : wav_files)
  {
    Sound s;
    std::cout << "Open sound file: " << wav_fn << " ";
    EXPECT_TRUE(s.Load(TEST_PATH + wav_fn));
    print_sound_info(s.get_info());
    std::cout << std::endl;
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
  SoundInfo target_quality;
  int i;

  target_quality.bitsize = 16;
  target_quality.channels = 2;
  target_quality.rate = 44100;
  i = 0;

  for (auto& wav_fn : wav_files)
  {
    std::cout << "Open sound file: " << wav_fn << " ";
    s[i].Resample(target_quality);
    s[i].Load(TEST_PATH + wav_fn);
    print_sound_info(s[i].get_info());
    std::cout << std::endl;
    i++;
  }

  EXPECT_STREQ("90 03 90 03 19 FE 19 FE ",
    get_data_in_hex((uint8_t*)s[0].get_ptr(), 8).c_str());

#if 0
  // just checking is_memory_valid? at last section of buffer
  print_data_in_hex((uint8_t*)(s_resample[0].ptr() + s_resample[0].buffer_byte_size() - 8), 8, std::cout);
  std::cout << std::endl;
#endif

  out.SetEmptyBuffer(target_quality, GetFrameFromMilisecond(5000, target_quality));

  s[0].Play(0);
  s[1].Play(0);
  s[2].Play(0);

  s[0].MixDataTo(out.get_ptr(), 999999);
  s[0].MixDataTo(out.get_ptr() + GetByteFromMilisecond(500, target_quality),
    999999);
  s[0].MixDataTo(out.get_ptr() + GetByteFromMilisecond(1200, target_quality),
    999999);
  s[1].MixDataTo(out.get_ptr() + GetByteFromMilisecond(800, target_quality),
    999999);
  s[1].MixDataTo(out.get_ptr() + GetByteFromMilisecond(1600, target_quality),
    999999);
  s[2].MixDataTo(out.get_ptr() + GetByteFromMilisecond(1400, target_quality),
    40980);

  EXPECT_STREQ("90 03 90 03 19 FE 19 FE ",
    get_data_in_hex((uint8_t*)out.get_ptr(), 8).c_str());

  EXPECT_TRUE(out.Save(TEST_PATH + "test_out.wav"));
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
    print_sound_info(s.get_info());
    std::cout << std::endl;
  }
  
  // just for test listening
  EXPECT_TRUE(s.Save(TEST_PATH + "test_out_ogg.wav"));
}

TEST(ENCODER, OGG)
{
  using namespace rmixer;
  auto wav_files = {
    "1-Loop-1-16.wav",
    "1-loop-2-02.wav",
  };
  SoundInfo target_quality;
  target_quality.bitsize = 16;
  target_quality.channels = 2;
  target_quality.rate = 44100;
  Sound s[2];
  Sound out;
  int i;

  // load
  i = 0;
  for (auto& wav_fn : wav_files)
  {
    s[i].Resample(target_quality);
    s[i++].Load(TEST_PATH + wav_fn);
  }

  // mix
  out.Resample(target_quality);
  out.SetEmptyBuffer(target_quality, GetFrameFromMilisecond(5000, target_quality));

  s[0].Play(0);
  s[1].Play(0);

  s[0].MixDataTo(out.get_ptr(), 999999);
  s[0].MixDataTo(out.get_ptr() + GetByteFromMilisecond(500, target_quality),
    999999);
  s[0].MixDataTo(out.get_ptr() + GetByteFromMilisecond(1200, target_quality),
    999999);
  s[1].MixDataTo(out.get_ptr() + GetByteFromMilisecond(800, target_quality),
    999999);
  s[1].MixDataTo(out.get_ptr() + GetByteFromMilisecond(1600, target_quality),
    999999);

  EXPECT_TRUE(out.Save(TEST_PATH + "test_out.ogg"));
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
    print_sound_info(s.get_info());
    std::cout << std::endl;
  }

  // just for test listing
  EXPECT_TRUE(s.Save(TEST_PATH + "test_out_mp3.wav"));
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
    print_sound_info(s.get_info());
    std::cout << std::endl;
  }

  // just for test listing
  EXPECT_TRUE(s.Save(TEST_PATH + "test_out_flac.wav"));
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
    print_sound_info(s.get_info());
    std::cout << std::endl;
  }

  EXPECT_TRUE(s.Save(TEST_PATH + "test_out_flac_enc.flac"));
}

TEST(BMS, BMS_ENCODING_ZIP)
{
  using namespace rmixer;

  /* prepare song & chart */
  rparser::Song song;
  ASSERT_TRUE(song.Open(TEST_PATH + u8"ìÑ¡¡ãó¡¡ÞÀ¡¡Íº¡¡ªÇ¡¡ïÎ¡¡ò­.zip"));
  rparser::Directory *songresource = song.GetDirectory();
  rparser::Chart *c = song.GetChart(0);
  ASSERT_TRUE(c);
  c->Invalidate();

  /* prepare SoundPool & mixer */
  SoundInfo mixinfo;
  mixinfo.bitsize = 16;
  mixinfo.rate = 44100;
  mixinfo.channels = 2;
  Mixer mixer(mixinfo);
  KeySoundPoolWithTime soundpool;
  const size_t channel_count = 2048;
  
  /* load resource & lane-channel mapping table */
  soundpool.Initalize(channel_count);
  soundpool.LoadFromChart(song, *c);
  soundpool.RegisterToMixer(mixer);

  /* misc setting: set volume */
  soundpool.SetVolume(0.8f);

  /* do mixing & save result with metadata */
  std::map<std::string, std::string> metadata;
  Sound s;

  auto &md = c->GetMetaData();
  metadata["TITLE"] = md.title;
  metadata["ARTIST"] = md.artist;
  soundpool.SetAutoPlay(true);
  soundpool.RecordToSound(s);
  EXPECT_TRUE(s.Save(
    TEST_PATH + "test_out_bms.ogg",
    metadata,
    0.6
  ));

  /* Cleanup */
  soundpool.Clear();
  song.CloseChart();
  song.Close();
}

TEST(SAMPLER, PITCH)
{
  // MUST precede BMS_ENCODING_ZIP test

  using namespace rmixer;
  Sound s;

  ASSERT_TRUE(s.Load(TEST_PATH + "test_out_bms.ogg"));
  EXPECT_TRUE(s.Resample(1.5, 1.0, 1.0));
  ASSERT_TRUE(s.Save(TEST_PATH + "test_out_bms_resample1.ogg"));
}

TEST(SAMPLER, TEMPO)
{
  // PITCH & TEMPO move, which results in PITCH shifting without changing duration.
  // MUST precede BMS_ENCODING_ZIP test

  using namespace rmixer;
  Sound s;

  ASSERT_TRUE(s.Load(TEST_PATH + "test_out_bms.ogg"));
  EXPECT_TRUE(s.Resample(1.0, 0.666, 1.0));
  ASSERT_TRUE(s.Save(TEST_PATH + "test_out_bms_resample2.ogg"));
}

TEST(MIXER, MIXING)
{
  // test for seamless real-time sound encoding
  using namespace rmixer;

  /* prepare song & chart */
  rparser::Song song;
  ASSERT_TRUE(song.Open(TEST_PATH + u8"ìÑ¡¡ãó¡¡ÞÀ¡¡Íº¡¡ªÇ¡¡ïÎ¡¡ò­.zip"));
  rparser::Directory *songresource = song.GetDirectory();
  rparser::Chart *c = song.GetChart(0);
  ASSERT_TRUE(c);
  c->Invalidate();

  /* prepare SoundPool & mixer */
  SoundInfo mixinfo;
  mixinfo.bitsize = 16;
  mixinfo.rate = 44100;
  mixinfo.channels = 2;
  Mixer mixer(mixinfo);
  KeySoundPoolWithTime soundpool;
  const size_t channel_count = 2048;

  /* load resource & lane-channel mapping table */
  soundpool.Initalize(channel_count);
  soundpool.LoadFromChart(song, *c);
  soundpool.RegisterToMixer(mixer);

  Sound out(mixinfo, 1024000);
  constexpr size_t kMixingByte = 1024;  /* 2BPS * 2CH * 10ms~=440Frame */
  size_t mixing_byte_offset = 0;
  const float delta_ms = GetMilisecondFromByte(kMixingByte, mixinfo);
  while (mixing_byte_offset < out.get_total_byte())
  {
    soundpool.Update(delta_ms);
    mixer.Mix((char*)out.get_ptr() + mixing_byte_offset, kMixingByte);
    mixing_byte_offset += kMixingByte;
  }

  EXPECT_TRUE(out.Save(TEST_PATH + "test_out_bms_mixer.ogg"));
}

TEST(MIXER, MIDI)
{
  /** midi mixing test with VOS file. */

  // prepare mixing
  using namespace rmixer;
  SoundInfo mixinfo;
  mixinfo.bitsize = 16;
  mixinfo.rate = 44100;
  mixinfo.channels = 2;
  Mixer mixer(mixinfo, (TEST_PATH + "midi.cfg").c_str());
  KeySoundPoolWithTime soundpool;
  soundpool.Initalize(128);

  // open song
  rparser::Song song;
  ASSERT_TRUE(song.Open(TEST_PATH + u8"1.vos"));
  rparser::Chart *c = song.GetChart(0);
  ASSERT_TRUE(c);
  c->Invalidate();

  // load chart into soundpool
  soundpool.LoadFromChart(song, *c);

  // mixing & save
  Sound s;
  soundpool.RegisterToMixer(mixer);
  soundpool.RecordToSound(s);
  EXPECT_TRUE(s.Save(TEST_PATH + "test_out_midi.ogg"));

  // cleanup
  song.Close();
  soundpool.Clear();
}

int main(int argc, char **argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  //::testing::FLAGS_gtest_filter = "MIXER.*";
  ::testing::FLAGS_gtest_catch_exceptions = 0;  // ASSERT to DEBUGGER
  return RUN_ALL_TESTS();
}
