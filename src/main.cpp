#include <stdio.h>
#include <string>
#include <Song.h>
#include <rutil.h>
#include <rutil.Sound.h>

#ifdef WIN32
int wmain(int argc, wchar_t **argv)
#else
int main(int argc, char **argv)
#endif
{
	// TEST PURPOSE
	wchar_t _asspath[] = L"D:\\BMS_TEST\\Angelico";
#ifdef WIN32
	std::string _bmspath;
	rutil::EncodeFromWStr(_asspath, _bmspath, R_CP_UTF8);
#else
	std::string _bmspath = argv[1];
#endif

	// load bms file
	rparser::Song song;
	if (!song.Open(_bmspath))
	{
		printf("Failed to open song file.\n");
		return -1;
	}

	// load bms resource for first chart
	std::vector<rparser::Chart*> vCharts;
	song.GetCharts(vCharts);
	rparser::Chart* curr_chart = vCharts[0];
	const rparser::SoundChannel* soundChannel = curr_chart->GetMetaData()->GetSoundChannel();

	std::map<int, rutil::SoundData> wavs;
	for (auto it = soundChannel->fn.begin(); it != soundChannel->fn.end(); ++it)
	{
		// attempt to retrieve file data
		const rutil::IDirectory* pDir = song.GetDirectory();
		rutil::FileData fd(it->second);
		if (!pDir->ReadSmart(fd))
		{
			printf("Failed to open sound file %s, ignore.\n", it->second.c_str());
			continue;
		}

		// attempt to decode WAV file data
		rutil::SoundData sd;
		bool bSound = rutil::LoadSound(fd.GetPtr(), fd.m_iLen, sd) == 0;
		//rutil::DeleteFileData(fd);
		if (!bSound)
		{
			printf("Failed to read sound file %s, ignore.\n", it->second.c_str());
			continue;
		}
		wavs[it->first] = sd;
	}

	// start mixing
	// TODO

    printf("hello world!");
	return 0;
}