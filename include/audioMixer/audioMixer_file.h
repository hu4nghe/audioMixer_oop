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

				auto soundFileConfig = outputConfig;
				soundFileConfig.queueCapacity = bufferSize;
				audioQueue<float> soundFileQueue(soundFileConfig);
				std::vector<float> audioData(temp, temp + bufferSize);

				soundFileQueue.push(audioData, sndFile.samplerate(), sndFile.channels());
				delete[] temp;

				audio->push_back(std::move(soundFileQueue));
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
				//open file and retrieve stream infos
				auto fmtCtx = avformat_alloc_context();
				avformat_open_input(&fmtCtx, videoFilePath.string().c_str(), nullptr, nullptr);
				avformat_find_stream_info(fmtCtx, nullptr);

				//for all stream found
				for (auto i = 0; i < fmtCtx->nb_streams; i++)
				{
					auto codecParam = fmtCtx->streams[i]->codecpar;
					if (codecParam->codec_type == AVMEDIA_TYPE_AUDIO)
					{
						auto audioStream = fmtCtx->streams[i];
						auto fileSampleRate = codecParam->sample_rate;
						auto fileNbChannels = codecParam->ch_layout.nb_channels;
						std::print("Channel : {}\nSample rate : {}\n",fileNbChannels, fileSampleRate);

						auto codec = avcodec_find_decoder(codecParam->codec_id);
						auto codecCtx = avcodec_alloc_context3(codec);
						avcodec_parameters_to_context(codecCtx, codecParam);
						avcodec_open2(codecCtx, codec, nullptr); 

						auto packet = av_packet_alloc();
						std::vector<std::uint8_t> dataVector;
						while (av_read_frame(fmtCtx, packet) >= 0)
						{
							if (packet->stream_index == audioStream->index)
							{
								
								auto frame = av_frame_alloc();
								auto ret = avcodec_send_packet(codecCtx, packet);

								while (ret >= 0)
								{
									ret = avcodec_receive_frame(codecCtx, frame);

									auto sampleSize = av_get_bytes_per_sample(codecCtx->sample_fmt);
									auto fmt = frame->format;
									auto sampleRate = frame->sample_rate;
									auto nbChannel = frame->ch_layout.nb_channels;
									std::print("sample size : {}\nsample rate : {}\nchannels : {}\nformat : {}\n////////////////////\n", 
												sampleSize,sampleRate,nbChannel,fmt);
									
									
									for (int i = 0; i < frame->nb_samples; i++)
									{
										for (int ch = 0; ch < codecCtx->ch_layout.nb_channels; ch++)
										{
											std::uint8_t* data_start = frame->data[ch] + sampleSize * i;
											dataVector.insert(dataVector.end(), data_start, data_start + sampleSize);
										}
									}
								}
								
							}
						}
						std::size_t floatSize = dataVector.size() / 4;
						float* floatData = reinterpret_cast<float*>(dataVector.data());
						std::vector<float> floatVec(floatData, floatData + floatSize);

						auto videoFileConfig = outputConfig;
						videoFileConfig.queueCapacity = floatSize;
						audioQueue<float> videoFileQueue(videoFileConfig);
						videoFileQueue.push(floatVec, 44100, 2);
						audio->push_back(std::move(videoFileQueue));
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
static void decode(AVCodecContext* dec_ctx, AVPacket* pkt, AVFrame* frame, std::vector<uint8_t>& audioSamples)
{
	int i, ch;
	int ret, data_size;

	//send the packet with the compressed data to the decoder 
	ret = avcodec_send_packet(dec_ctx, pkt);
	if (ret < 0)
	{
		fprintf(stderr, "Error submitting the packet to the decoder\n");
		exit(1);
	}

	//read all the output frames (in general there may be any number of them 
	while (ret >= 0)
	{
		ret = avcodec_receive_frame(dec_ctx, frame);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
			return;
		else if (ret < 0)
		{
			fprintf(stderr, "Error during decoding\n");
			exit(1);
		}
		data_size = av_get_bytes_per_sample(dec_ctx->sample_fmt);
		if (data_size < 0)
		{
			//This should not occur, checking just for paranoia
			fprintf(stderr, "Failed to calculate data size\n");
			exit(1);
		}
		for (i = 0; i < frame->nb_samples; i++)
			for (ch = 0; ch < dec_ctx->channels; ch++)
				audioSamples.push_back(frame->data[ch][data_size * i]);
	}
}*/