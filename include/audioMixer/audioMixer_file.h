#ifndef AUDIOMIXER_FILE_H
#define AUDIOMIXER_FILE_H

#include "sndfile.hh"

//ffmpeg headers
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
		//create a audioQueue for each sound file.
		for (auto& soundFile : soundList)
		{
			audioQueue<float> soundFileAudioQueue(outputConfig);
			audio->push_back(std::move(soundFileAudioQueue));
		}
		//zip sound file list and audioQueue list together.
		auto fileQueueMap = std::views::zip(soundList, (*audio));

		std::for_each(
			std::execution::par,//parallel execution
			fileQueueMap.begin(),
			fileQueueMap.end(),
			[this](const auto& tuple)//lambda that takes a mapped pair as parameters
			{
				//structure binding into mapped pairs (file -> audioQueue)
				auto&& [file, queue] = tuple;

				//Create sndfile handle
				SndfileHandle sndFile(file.string());
				const auto audioFormatCode = sndFile.format() & SF_FORMAT_SUBMASK;
				const auto chunkBufferSize = sndFile.samplerate(); // 1 second 

				//file reading loop
				bool quit = false;
				while (!quit)
				{
					//PCM 16bit short
					if (audioFormatCode == SF_FORMAT_PCM_16)
					{
						auto shortBuff = std::make_unique<short[]>(chunkBufferSize);
						auto floatBuff = std::make_unique<float[]>(chunkBufferSize);
						while (!quit)
						{
							//only read when there are no enough samples.
							if (queue.size() < outputConfig.minimumElement * outputConfig.channelNumber)
							{
								if (!sndFile.read(shortBuff.get(), chunkBufferSize)) 
									quit = true;//EOF
								else
								{
									src_short_to_float_array(shortBuff.get(), floatBuff.get(), chunkBufferSize);
									queue.push(std::vector<float>(floatBuff.get(), floatBuff.get() + chunkBufferSize),
											   sndFile.samplerate(),
											   sndFile.channels());
								}
							}
						}
					}
					//PCM 32bit float
					else if (audioFormatCode == SF_FORMAT_PCM_32)
					{
						auto floatBuff = std::make_unique<float[]>(chunkBufferSize);
						while (!quit)
						{
							//only read when there are no enough samples.
							if (queue.size() < outputConfig.minimumElement * outputConfig.channelNumber)
							{
								if (!sndFile.read(floatBuff.get(), chunkBufferSize)) 
									quit = true;//EOF
								else
									queue.push(std::vector<float>(floatBuff.get(), floatBuff.get() + chunkBufferSize),
											   sndFile.samplerate(),
										       sndFile.channels());
							}
						}
					}
				}
				std::print("File end : {}\n", file.filename().string());
			});
	}
	
	void readVideoFiles()
	{
		//create a audioQueue for each video file.
		for (auto& videoFile : videoList)
		{
			audioQueue<float> videoFileAudioQueue(outputConfig);
			audio->push_back(std::move(videoFileAudioQueue));
		}

		auto videoQueueView = (*audio) | std::views::drop(soundList.size()) | std::views::take(videoList.size());

		//zip video file list and audioQueue list together.
		auto fileQueueMap = std::views::zip(videoList, videoQueueView);

		std::for_each(
			std::execution::par,
			fileQueueMap.begin(),
			fileQueueMap.end(),
			[this](const auto& tuple)
			{
				//structure binding into mapped pairs (file -> audioQueue)
				auto&& [file, queue] = tuple;

				//open file and retrieve stream infos
				auto fmtCtx = avformat_alloc_context();
				avformat_open_input(&fmtCtx, file.string().c_str(), nullptr, nullptr);
				avformat_find_stream_info(fmtCtx, nullptr);

				//for all stream found
				for (auto i = 0; i < fmtCtx->nb_streams; i++)
				{
					auto codecParam = fmtCtx->streams[i]->codecpar;
					if (codecParam->codec_type == AVMEDIA_TYPE_AUDIO)
					{
						auto audioStream = fmtCtx->streams[i];
						auto codec		 = avcodec_find_decoder(codecParam->codec_id);
						auto codecCtx	 = avcodec_alloc_context3(codec);
						avcodec_parameters_to_context(codecCtx, codecParam);
						avcodec_open2(codecCtx, codec, nullptr); 
						
						codecCtx->request_sample_fmt = AV_SAMPLE_FMT_FLTP;

						int audioFormat = 0;
						std::vector<std::uint8_t> byteData;

						auto packet = av_packet_alloc();
						bool eof = false;
						while (!eof)
						{
							if (queue.size() < outputConfig.minimumElement * outputConfig.channelNumber)
							{
								av_read_frame(fmtCtx, packet);
								if (packet->stream_index == audioStream->index)
								{
									auto frame = av_frame_alloc();
									auto ret = avcodec_send_packet(codecCtx, packet);

									while (ret >= 0)
									{
										ret = avcodec_receive_frame(codecCtx, frame);
										if (ret == AVERROR(EAGAIN))
											break;
										else if (ret == AVERROR_EOF)
										{
											std::print("EOF.\n");
											eof = true;
											break;
										}
										else if (ret < 0)
										{
											std::print("Error reading file.\n");
											break;
										}
										auto sampleSize = av_get_bytes_per_sample(codecCtx->sample_fmt);

										audioFormat = frame->format;

										const auto bufferSize = frame->nb_samples * codecCtx->ch_layout.nb_channels * sampleSize;
										byteData.reserve(sampleSize);

										for (int i = 0; i < frame->nb_samples; i++)
											for (int ch = 0; ch < codecCtx->ch_layout.nb_channels; ch++)
											{
												std::uint8_t* data_start = frame->data[ch] + sampleSize * i;
												byteData.insert(byteData.end(), data_start, data_start + sampleSize);
											}
										if (byteData.size() == 0)
											break;
										std::size_t floatSize = byteData.size() / 4;
										std::print("pushed size :{}\n", floatSize);
										float* floatData = reinterpret_cast<float*>(byteData.data());
										queue.push(std::vector<float>(floatData, floatData + floatSize), 44100, 2);
										byteData.clear();
									}
								}
							}
						}
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
		std::print("Press any key to restart.\n");
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
