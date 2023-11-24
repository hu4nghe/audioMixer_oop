#ifndef AUDIOMIXER_NDI_RECV_H
#define AUDIOMIXER_NDI_RECV_H

#include "Processing.NDI.Lib.h"

#include <iostream>
#include <ranges>


#include "audioMixer_base.h"


class NDIFinder
{
	NDIlib_find_instance_t finder{};
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
   ~NDIFinder() 
	{ 
		NDIlib_find_destroy(finder); 
	}

    auto findSrc(std::uint32_t timeout) -> std::vector<NDIlib_source_t>
	{
		const NDIlib_source_t* source		{ nullptr };
				std::uint32_t  sourceNumber	{ 0 };
						 bool  found		{ false };

		//try to find sources using this->finder.
		while (!found)
		{
			NDIlib_find_wait_for_sources(finder, timeout);
			source = NDIlib_find_get_current_sources(finder, &sourceNumber);
			found  = true;
		}

		std::vector<NDIlib_source_t> sourcesFound(sourceNumber);

		//push sources into result vector
		for (std::uint32_t i = 0; i < sourceNumber; i++)
			sourcesFound[i] = source[i];

		return sourcesFound;
	}
};

class NDIReceiver
{
	NDIlib_recv_instance_t	receiver   {};
	NDIlib_audio_frame_v2_t audioFrame {};
public:
	NDIReceiver() = default;
	NDIReceiver(const NDIlib_source_t& source)
	{
		if (source.p_url_address == nullptr)
			throw std::runtime_error("Invalide NDI source");
		else
		{
			NDIlib_recv_create_v3_t recvConfig {};
			recvConfig.p_ndi_recv_name = source.p_ndi_name;
			recvConfig.source_to_connect_to = source;
			receiver = NDIlib_recv_create_v3(&recvConfig);
			if (receiver == nullptr)
				throw std::runtime_error("Unable to create NDI receiver.");
		}
	}
   ~NDIReceiver(){ NDIlib_recv_destroy(receiver); }

	template<audioType T>
	void getAudio(audioQueue<T>& queue)
	{
		if (NDIlib_recv_capture_v2(receiver, nullptr, &audioFrame, nullptr, 0) == NDIlib_frame_type_audio)
		{
			const auto dataSize { static_cast<std::size_t>(audioFrame.no_samples) * audioFrame.no_channels };
			if (std::is_same<T, float>::value)
			{
				NDIlib_audio_frame_interleaved_32f_t audio32f {};
				audio32f.p_data = new T[dataSize];
				NDIlib_util_audio_to_interleaved_32f_v2(&audioFrame, &audio32f);
				std::vector<T> audioVec(audio32f.p_data, audio32f.p_data + dataSize);
				if(queue.push(audioVec, audio32f.sample_rate, audio32f.no_channels))
					std::print("Push failed : no enough space in the queue !\n");
			}
			else
			{
				NDIlib_audio_frame_interleaved_16s_t audio16s{};
				audio16s.p_data = new T[dataSize];
				NDIlib_util_audio_to_interleaved_16s_v2(&audioFrame, &audio16s);
				std::vector<T> audioVec(audio16s.p_data, audio16s.p_data + dataSize);
				if (queue.push(audioVec, audio16s.sample_rate, audio16s.no_channels))
					std::print("Push failed : no enough space in the queue !\n");
			}
		}
		else
			std::print("No audio data is received.\n");
	}
};

class NDI : public module
{
	/*NDI Receivers*/
	std::vector<NDIReceiver> recvList;

	void srcSearch();
	void recvAudio();
public:
	NDI(const outputParameter outputCfg);

    void start() override;
    void stop () override;
};

#pragma region IMPL
	 NDI::NDI	   (const outputParameter outputCfg)
	:	module(outputCfg) {}
void NDI::start	   ()
{
	if (!NDIlib_initialize()) 
		throw std::runtime_error("Cannot run NDI.");
	else
	{
		std::print("NDI Module is activated.\n");
		active = true;
	}
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

	constexpr auto timeout = 1000;
	NDIFinder audioFinder;
	auto sourcesFound = audioFinder.findSrc(timeout);

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
		for (auto [receiver, audioData] : std::views::zip(recvList, (*audio)))
			receiver.getAudio(audioData);
}
#pragma endregion
//IMPL class NDI
#endif//NDI_RECV_MODULE_H