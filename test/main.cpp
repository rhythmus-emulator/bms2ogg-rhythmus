#include <iostream>
#include <gtest/gtest.h>
#include "Decoder.h"
#include "Encoder.h"
#include "Sampler.h"

#define TEST_PATH std::string("../test/test/")

inline void print_sound_info(const rhythmus::SoundInfo &info)
{
  std::cout << "(ch" << (int)info.channels << ", bps" << info.bitsize << ", rate" << info.rate << ")";
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
  };
  Sound s[2];
  Sound s_resample[2];
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
    print_sound_info(s[i-1].get_info());
    std::cout << std::endl;
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
  mixer.Mix(s_resample[0], 1000);
  mixer.Mix(s_resample[0], 1500);
  mixer.Mix(s_resample[0], 2000);
  mixer.Mix(s_resample[1], 1100);
  mixer.Mix(s_resample[1], 1300);
  mixer.Mix(s_resample[1], 1600);

  // TODO: add metatag info to encoder
  Encoder_WAV encoder(mixer);
  encoder.Write(TEST_PATH + "test_out.wav");
  encoder.Close();
}

int main(int argc, char **argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
