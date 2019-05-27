#include <iostream>
#include <gtest/gtest.h>
#include "Decoder.h"
#include "Encoder.h"

#define TEST_PATH std::string("../test/test/")

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
    std::cout << "Open sound file: " << wav_fn << std::endl;
    rutil::FileData fd = rutil::ReadFileData(TEST_PATH + wav_fn);
    ASSERT_TRUE(fd.len > 0);
    EXPECT_TRUE(wav.open(fd));
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

  int i = 0;
  for (auto& wav_fn : wav_files)
  {
    Decoder_WAV wav(s[i++]);
    std::cout << "Open sound file: " << wav_fn << std::endl;
    rutil::FileData fd = rutil::ReadFileData(TEST_PATH + wav_fn);
    ASSERT_TRUE(fd.len > 0);
    EXPECT_TRUE(wav.open(fd));
  }

  SoundMixer mixer;
  mixer.Mix(s[0], 1000);
  mixer.Mix(s[0], 1500);
  mixer.Mix(s[0], 2000);
  mixer.Mix(s[1], 1100);
  mixer.Mix(s[1], 1300);
  mixer.Mix(s[1], 1600);

  Encoder_WAV encoder(mixer);
  encoder.Write(TEST_PATH + "test_out.wav");
  encoder.Close();
}

int main(int argc, char **argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
