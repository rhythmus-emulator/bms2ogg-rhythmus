#include <iostream>
#include <gtest/gtest.h>
#include "Decoder.h"
#include "Encoder.h"
#include "Sampler.h"
#include "Song.h"     // rparser

#define TEST_PATH std::string("../test/test/")

inline void print_sound_info(const rhythmus::SoundInfo &info)
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
  using namespace rhythmus;
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
    Decoder_WAV wav(s);
    std::cout << "Open sound file: " << wav_fn << " ";
    rutil::FileData fd = rutil::ReadFileData(TEST_PATH + wav_fn);
    ASSERT_TRUE(fd.len > 0);
    EXPECT_TRUE(wav.open(fd));
    wav.read();
    print_sound_info(s.get_info());
    std::cout << std::endl;
  }
}

TEST(ENCODER, WAV)
{
  using namespace rhythmus;
  auto wav_files = {
    "1-Loop-1-16.wav",
    "1-loop-2-02.wav",
    "8kadpcm.wav",  // 4bit adpcm wav test
  };
  Sound s[3];
  Sound s_resample[3];
  int i;

  i = 0;
  for (auto& wav_fn : wav_files)
  {
    Decoder_WAV wav(s[i++]);
    std::cout << "Open sound file: " << wav_fn << " ";
    rutil::FileData fd = rutil::ReadFileData(TEST_PATH + wav_fn);
    ASSERT_TRUE(fd.len > 0);
    EXPECT_TRUE(wav.open(fd));
    wav.readAsS32();  // just test for converting from 32bit sound
    //wav.read();
    print_sound_info(s[i-1].get_info());
    std::cout << std::endl;
  }

  // need resampler before mixing
  SoundInfo target_quality;
  target_quality.bitsize = 16;
  target_quality.channels = 2;
  target_quality.rate = 44100;

  EXPECT_STREQ("00 00 90 03 00 00 07 FC ",
    get_data_in_hex((uint8_t*)s[0].ptr(), 8).c_str());

  i = 0;
  for (auto& wav : s)
  {
    Sampler sampler(wav, target_quality);
    EXPECT_TRUE(sampler.Resample(s_resample[i++]));
    wav.Clear();  // remove old sample
  }

#if 0
  // just checking is_memory_valid? at last section of buffer
  print_data_in_hex((uint8_t*)(s_resample[0].ptr() + s_resample[0].buffer_byte_size() - 8), 8, std::cout);
  std::cout << std::endl;
#endif

  EXPECT_STREQ("90 03 90 03 19 FE 19 FE ",
    get_data_in_hex((uint8_t*)s_resample[0].ptr(), 8).c_str());

  SoundMixer mixer;
  mixer.SetInfo(target_quality);
  mixer.Mix(s_resample[0], 0);
  mixer.Mix(s_resample[0], 500);
  mixer.Mix(s_resample[0], 1200);
  mixer.Mix(s_resample[0], 2000);
  mixer.Mix(s_resample[1], 800);
  mixer.Mix(s_resample[1], 1600);
  mixer.Mix(s_resample[2], 1400);

  EXPECT_STREQ("90 03 90 03 19 FE 19 FE ",
    get_data_in_hex((uint8_t*)mixer.get_chunk(0), 8).c_str());

  Encoder_WAV encoder(mixer);
  encoder.SetMetadata("TITLE", "test");
  encoder.SetMetadata("ARTIST", "test_artist");
  encoder.Write(TEST_PATH + "test_out.wav");
  encoder.Close();
}

TEST(DECODER, OGG)
{
  using namespace rhythmus;
  auto wav_files = {
    "m09.ogg",
  };
  Sound s;

  for (auto& wav_fn : wav_files)
  {
    Decoder_OGG ogg(s);
    std::cout << "Open sound file: " << wav_fn << " ";
    rutil::FileData fd = rutil::ReadFileData(TEST_PATH + wav_fn);
    ASSERT_TRUE(fd.len > 0);
    ASSERT_TRUE(ogg.open(fd));
    ogg.read();
    print_sound_info(s.get_info());
    std::cout << std::endl;
  }
  
  // just for test listing
  Encoder_WAV encoder(s);
  encoder.Write(TEST_PATH + "test_out_ogg.wav");
  encoder.Close();
}

TEST(ENCODER, OGG)
{
  using namespace rhythmus;
  auto wav_files = {
    "1-Loop-1-16.wav",
    "1-loop-2-02.wav",
  };
  Sound s[2];
  Sound s_resample[2];
  int i;

  i = 0;
  for (auto& wav_fn : wav_files)
  {
    Decoder_WAV wav(s[i++]);
    rutil::FileData fd = rutil::ReadFileData(TEST_PATH + wav_fn);
    ASSERT_TRUE(fd.len > 0);
    EXPECT_TRUE(wav.open(fd));
    wav.read();
  }

  // need resampler before mixing
  SoundInfo target_quality;
  target_quality.bitsize = 16;
  target_quality.channels = 2;
  target_quality.rate = 44100;

  i = 0;
  for (auto& wav : s)
  {
    Sampler sampler(wav, target_quality);
    EXPECT_TRUE(sampler.Resample(s_resample[i++]));
    wav.Clear();  // remove old sample
  }

  SoundMixer mixer;
  mixer.SetInfo(target_quality);
  mixer.Mix(s_resample[0], 0);
  mixer.Mix(s_resample[0], 500);
  mixer.Mix(s_resample[0], 1200);
  mixer.Mix(s_resample[0], 2000);
  mixer.Mix(s_resample[1], 800);
  mixer.Mix(s_resample[1], 1600);

  Encoder_OGG encoder(mixer);
  encoder.SetMetadata("TITLE", "test");
  encoder.SetMetadata("ARTIST", "test_artist");
  encoder.Write(TEST_PATH + "test_out.ogg");
  encoder.Close();
}

TEST(SAMPLER, PITCH)
{
}

TEST(SAMPLER, TEMPO)
{
}

TEST(BMS, BMS_ENCODING_ZIP)
{
  rparser::Song song;
  ASSERT_TRUE(song.Open(TEST_PATH + u8"ìÑ¡¡ãó¡¡ÞÀ¡¡Íº¡¡ªÇ¡¡ïÎ¡¡ò­.zip"));
  rparser::Directory *songresource = song.GetDirectory();

  constexpr size_t kMaxSoundChannel = 2048;
  rhythmus::Sound sound_channel[kMaxSoundChannel];
  rhythmus::SoundMixer mixer;
  rhythmus::SoundInfo mixinfo;
  mixinfo.bitsize = 16;
  mixinfo.rate = 44100;
  mixinfo.channels = 2;
  mixer.SetInfo(mixinfo);

  {
    /** Read chart and do time calculation */
    rparser::Chart *c = song.GetChart(0);
    ASSERT_TRUE(c);
    auto &md = c->GetMetaData();
    md.SetMetaFromAttribute();
    c->InvalidateTempoData();
    c->InvalidateAllNotePos();

    /** Read and decode necessary sound files */
    for (auto &ii : md.GetSoundChannel()->fn)
    {
      rhythmus::Sound s_temp;
      std::cout << "resource name is : " << ii.second << " (ch" << ii.first << ")" << std::endl;
      auto* fd = songresource->Get(ii.second, true);
      EXPECT_TRUE(fd);

      rhythmus::Decoder_WAV dWav(s_temp);
      dWav.open(*fd);
      dWav.read();

      ASSERT_TRUE(ii.first < kMaxSoundChannel);
      rhythmus::Sampler sampler(s_temp, mixinfo);
      sampler.Resample(sound_channel[ii.first]);
    }

    /** Start mixing (TODO: longnote) */
    auto &nd = c->GetNoteData();
    for (auto &n : nd)
    {
      EXPECT_TRUE(mixer.Mix(sound_channel[n.value], n.time_msec));
    }

    /* Save mixing result with metadata */
    rhythmus::Encoder_OGG encoder(mixer);
    encoder.SetMetadata("TITLE", md.title);
    encoder.SetMetadata("ARTIST", md.artist);
    encoder.Write(TEST_PATH + "test_out_bms.ogg");
    encoder.Close();

    /** Everything is done, close chart. */
    song.CloseChart();
  }

  song.Close();
}

int main(int argc, char **argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
