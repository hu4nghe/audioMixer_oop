#ifndef AUDIOMIXER_FILE_H
#define AUDIOMIXER_FILE_H

#include "sndfile.hh"

#include <filesystem>
#include <iostream>

#include "audioMixer_base.h"

namespace fs = std::filesystem;

class soundFile : public module
{
	/*Sound file path*/
	std::vector<fs::path> pathList;
	
	void selectFile();
	void readFile  ();

public:
	soundFile(const outputParameter& outputCfg);		

	void start() override;
	void stop () override;
};

#pragma region IMPL
	 soundFile::soundFile (const outputParameter& outputCfg)
	:	module(outputCfg) {}
void soundFile::start	  ()
{
	std::print("file Module is activated.\n");
	active = true;
	this->selectFile();
	this->readFile  ();
	while(!this->audio->empty()){}
}
void soundFile::stop	  ()
{
	std::print("file Module is stopped.\n");
	active = false;
}
void soundFile::selectFile()
{
	std::print("Please enter the path of the sound file, enter end to confirm.\n");
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
			}
		}
	} while (true);
}
void soundFile::readFile  ()
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

		sndQueue.push(audioData, i.samplerate(), i.channels());
		delete[] temp;

		audio->push_back(std::move(sndQueue));
	}
	return;
}
#pragma endregion
//IMPL soundFile
#endif//AUDIOMIXER_FILE_H