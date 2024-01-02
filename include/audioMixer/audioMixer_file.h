#ifndef AUDIOMIXER_FILE_H
#define AUDIOMIXER_FILE_H

#include "sndfile.hh"
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
}

#include <algorithm>
#include <execution>
#include <filesystem>
#include <iostream>
#include <fstream>

#include "audioMixer_base.h"

namespace fs = std::filesystem;

class soundFile : public module
{
	/*Sound file path*/
	std::vector<fs::path> soundList;
	std::vector<fs::path> videoList;
public:
	soundFile(const outputParameter& outputCfg) : module(outputCfg) {}
	void selectFile()
	{
		std::print("Please enter the path of the sound file.\nE(nd)\n");
		std::string filePathStr;
		do
		{
			std::getline(std::cin >> std::ws, filePathStr);
			if (filePathStr == "E" || filePathStr == "e")
			{
				std::print("File path confimed.\n");
				break;
			}
			else
			{
				fs::path filePath(filePathStr);
				if (fs::exists(filePath))
				{
					const auto extension = filePath.extension();
					if (extension == ".wav")
						soundList.push_back(filePath);
					else if (extension == ".mov" || extension == ".mp4")
						videoList.push_back(filePath);
					else
					{
						std::print("Format not supported.");
						continue;
					}
					std::print("File seletcted : \n{}.\n", filePath.filename().string());
				}
				else
					std::print("No such file or dictory.\n");
			}
		} while (true);
	}
	
	void readSoundFiles()
	{
		std::for_each(
			std::execution::par,
			soundList.begin(),
			soundList.end(), 
			[this](const fs::path& sndFilePath) 
			{
				SndfileHandle sndFile(sndFilePath.string());
				const std::size_t bufferSize = sndFile.frames() * sndFile.channels() + 10;
				float* temp = new float[bufferSize];
				sndFile.read(temp, bufferSize);

				outputConfig.queueCapacity = bufferSize;
				audioQueue<float> sndQueue(outputConfig);
				std::vector<float> audioData(temp, temp + bufferSize);

				sndQueue.push(audioData, sndFile.samplerate(), sndFile.channels());
				delete[] temp;

				audio->push_back(std::move(sndQueue));
			});
	}
	
	void readVideoFiles()
	{
		std::for_each(
			std::execution::par,
			videoList.begin(),
			videoList.end(),
			[this](const fs::path& videoFilePath)
			{
				auto fmtCtx = avformat_alloc_context();
				avformat_open_input(&fmtCtx, videoFilePath.string().c_str(), nullptr, nullptr);
				avformat_find_stream_info(fmtCtx, nullptr);
				for (auto i = 0; i < fmtCtx->nb_streams; i++)
				{
					auto codecParam = fmtCtx->streams[i]->codecpar;
					if (codecParam->codec_type == AVMEDIA_TYPE_AUDIO)
					{
						auto audioStream = fmtCtx->streams[i];
						std::print("index : {}\n", audioStream->index);


						auto codec = avcodec_find_decoder(codecParam->codec_id);

						auto codecCtx = avcodec_alloc_context3(codec);
						avcodec_parameters_to_context(codecCtx, codecParam);

						avcodec_open2(codecCtx, codec, nullptr);
						std::print("channels : {}\n", codec->long_name);


						auto packet = av_packet_alloc();
						auto frame = av_frame_alloc();

						auto ret = avcodec_send_packet(codecCtx, packet);
						
						std::vector<uint8_t> dataVector;
						while (ret >= 0)
						{
							ret = avcodec_receive_frame(codecCtx, frame);
							auto data_size = av_get_bytes_per_sample(codecCtx->sample_fmt);
							std::print("data size : {}\n", data_size);
							
							for (int i = 0; i < frame->nb_samples; i++)
							{
								for(int ch = 0; ch < codecCtx->ch_layout.nb_channels;ch++)
								{
									uint8_t* data_start = frame->data[ch] + data_size * i;
									dataVector.insert(dataVector.end(), data_start, data_start + data_size);
								}
							}


						}
						std::print("packet all sent, data vector size : {}\n",dataVector.size());
					}

				}

				//C:\Users\Modulo\Desktop\Nouveau_dossier\Music\TRAIN.mov
			});
	}
	void start() override
	{
		std::print("file Module is activated.\n");
		active = true;
		soundList.clear();
		videoList.clear();
		selectFile();
		readSoundFiles();
		readVideoFiles();
		bool stop = false;
		while (!stop) 
		{
			stop = true;
			for (const auto& queue : *audio)
				if (!queue.empty())
					stop = false;
		}
		std::print("Press any key to try again.\n");
		std::cin.ignore();
		start();
	}
	void stop () override
	{
		std::print("file Module is stopped.\n");
		active = false;
	}
};

#endif//AUDIOMIXER_FILE_H
/*
catch (const modObjNotFound& err)
{
	std::print("File module error : {}.\n", err.what());
	std::print("Press any key to try again.\n");
	std::cin.ignore();
}
catch (const modFatalErr& err)
{
	std::print("File module fatal error : {}\n", err.what());
	throw err;
}*/