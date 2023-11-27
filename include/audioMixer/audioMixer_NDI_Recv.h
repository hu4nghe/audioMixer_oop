#ifndef AUDIOMIXER_NDI_RECV_H
#define AUDIOMIXER_NDI_RECV_H

#include "Processing.NDI.Lib.h"

#include <iostream>
#include <ranges>


#include "audioMixer_base.h"

constexpr auto NDI_TIMEOUT { 1000 };

class NDIFinder
{
	NDIlib_find_instance_t finder {};
public:
	NDIFinder()
	{
		NDIlib_find_create_t findCfg;
		finder = NDIlib_find_create_v2(&findCfg);
		if (!finder)//nullptr in case of failure.
			throw std::runtime_error("Failed to create NDI finder !");
	}
	NDIFinder(const bool  showLocalSources,
			  const char* pGroups)
	{
		NDIlib_find_create_t findCfg;
		//configure finder
		findCfg.show_local_sources = showLocalSources;
		findCfg.p_groups		   = pGroups;
		findCfg.p_extra_ips		   = nullptr; //deprecated feature

		finder = NDIlib_find_create_v2(&findCfg);
		if (!finder)//nullptr in case of failure.
			throw std::runtime_error("Failed to create NDI finder !");
	}
   ~NDIFinder(){ NDIlib_find_destroy(finder); }

    auto findSrc(std::uint32_t NDI_TIMEOUT) -> std::vector<NDIlib_source_t>
	{
		const NDIlib_source_t* sources		{ nullptr };
				std::uint32_t  sourceNumber	{ 0 };
						 bool  found		{ false };

		//try to find sources
		while (!found)
		{
			NDIlib_find_wait_for_sources(finder,NDI_TIMEOUT);
			sources = NDIlib_find_get_current_sources(finder, &sourceNumber);
			found   = true;
		}

		std::vector<NDIlib_source_t> sourcesFound(sources,sources+sourceNumber);
		return sourcesFound;
	}
};

class NDIReceiver
{
	NDIlib_recv_instance_t receiver;
public:
	NDIReceiver(const NDIlib_source_t& src)
	{
		NDIlib_recv_create_v3_t recvConfig;
		recvConfig.p_ndi_recv_name = src.p_ndi_name;
		recvConfig.source_to_connect_to = src;
		receiver = NDIlib_recv_create_v3(&recvConfig);
	}

	void getAudio(audioQueue<float>& audio)
	{
		NDIlib_audio_frame_v2_t audioInput;
		//receive only audio
		if (NDIlib_recv_capture_v2(receiver, nullptr, &audioInput, nullptr, 0) == NDIlib_frame_type_audio)
		{
			const auto dataSize = static_cast<size_t>(audioInput.no_samples) * audioInput.no_channels;
			NDIlib_audio_frame_interleaved_32f_t audio32f;
			audio32f.p_data = new float[dataSize];
			NDIlib_util_audio_to_interleaved_32f_v2(&audioInput, &audio32f);
			std::vector<float> audioData(audio32f.p_data, audio32f.p_data + dataSize);
			if (!audio.push(audioData, audio32f.sample_rate, audio32f.no_channels))
				std::print("No more space in the queue!\n");

			delete[] audio32f.p_data;
		}
	}
};
class NDI : public module
{
	/*NDI Receivers*/
	std::vector<NDIReceiver> recvList;
public:
	NDI(const outputParameter outputCfg);

    void start() override;
    void stop () override;

	void srcSearch();
	void recvAudio();
};

#pragma region IMPL
	 NDI::NDI	   (const outputParameter outputCfg)
	:	module(outputCfg) {}
void NDI::start	   ()
{
	NDIlib_initialize();
	std::print("NDI Module is activated.\n");
	active = true;
	this->srcSearch(); 
	this->recvAudio();
}
void NDI::stop	   ()
{
	NDIlib_destroy();
	std::print("NDI Module is stopped.\n");
	active = false;
}
void NDI::srcSearch()
{
	if (!active)
		throw std::logic_error("NDI lib is not running !");

			  NDIFinder audioFinder  {};
			       auto sourcesFound { audioFinder.findSrc(NDI_TIMEOUT) };

	//print all sources found
	std::print("NDI sources list:\n");
	for (auto& i : sourcesFound)
		std::print("Name : {}\nIP   : {}\n\n", i.p_ndi_name, i.p_url_address);

	//let user select NDI sources that they want.

	std::string url			  {};
		   bool sourceMatched { false };
		   bool selectAll	  { false };
	
	std::print("Please enter the IP of the source that you want to connect to, enter end to confirm.\n");
	do
	{
		sourceMatched = false;
		std::cin >> url;
		if (url == "all") selectAll = true;

		for (auto& i : sourcesFound)
		{
			if (url == i.p_url_address || selectAll)
			{
				recvList.push_back(NDIReceiver(i));
				std::print("{} selected.\n", i.p_ndi_name);
				sourceMatched = true;
			}
		}
		if (url == "end" || selectAll)
		{
			std::print("Sources confimed.\n");
			break;
		}
		if (!sourceMatched) std::print("No source matched! Please try again.\n");
	} while (true);
	//create a audioQueue for each selected NDI receiver.
	audio->resize(recvList.size(), audioQueue<float>(outputConfig));
}
void NDI::recvAudio()
{
	while (true)
		for(auto [recver,audio] : std::views::zip(recvList, (*audio)))
			recver.getAudio(audio);
}
#pragma endregion
//IMPL NDI Recv

#endif//NDI_RECV_MODULE_H