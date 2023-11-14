#ifndef AUDIOMIXER_FILE_H
#define AUDIOMIXER_FILE_H

#include <filesystem>
#include <iostream>
#include <print>

#include "audioMixer_queue.h"
#include "sndfile.hh"

namespace fs = std::filesystem;

class soundFile : public module
{
	friend class audioMixer;
	/*Sound file path*/
	std::vector<fs::path>           pathList;
	
	void selectAudioFile();
	void readAudioFile  ();

public:
	soundFile(const outputParameter& outputCfg);		

	void start() override;
	void stop () override;
};

#pragma region IMPL
inline soundFile::soundFile(const outputParameter& outputCfg)
	:	module(outputCfg) {}
inline void soundFile::start()
{
	std::print("file Module is activated.\n");
	std::jthread receiveThread([this](){ this->selectAudioFile();
										 this->readAudioFile  (); });
}
inline void soundFile::stop()
{
	std::print("file Module is stopped.\n");
}
void soundFile::selectAudioFile()
{
	std::print("Please enter the path of the sound file, enter end to confirm.");
	std::string filePathStr;
	do
	{
		std::getline(std::cin >> std::ws, filePathStr);
		if (filePathStr == "end")
		{
			std::print("Sound files confimed.\n");
			break;
		}
		else
		{
			fs::path filePath(filePathStr);
			if (fs::exists(filePath))
			{
				std::print("Sound file seletcted : {}.\n", filePath.filename().string());
				pathList.push_back(filePath);
			}
			else
			{
				std::print("Error : No such file or dictory.\n");
				continue;
			}
		}
	} while (true);
}
void soundFile::readAudioFile()
{
	std::vector<SndfileHandle> fileHandleList;
	for (auto& i : pathList)
	{
		SndfileHandle sndFile(i.string());
		fileHandleList.push_back(sndFile);
	}

	for (auto& i : fileHandleList)
	{
		const std::size_t bufferSize = i.frames() * i.channels() + 100;
		float* temp = new float[bufferSize];
		i.read(temp, bufferSize);

		outputConfig.queueCapacity = bufferSize;
		audioQueue<float> sndQueue(outputConfig);
		std::vector<float> audioData(temp, temp + bufferSize);

		sndQueue.push(std::move(audioData), i.samplerate(), i.channels());
		delete[] temp;

		audio->push_back(std::move(sndQueue));
	}
	return;
}
#pragma endregion
#endif//AUDIOMIXER_FILE_H