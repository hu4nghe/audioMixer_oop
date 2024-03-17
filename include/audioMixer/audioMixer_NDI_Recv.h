#ifndef AUDIOMIXER_NDI_RECV_H
#define AUDIOMIXER_NDI_RECV_H

#include <iostream>
#include <ranges>

#include "audioMixer_base.h"
#include "Processing.NDI.Lib.h"

constexpr auto NDI_TIMEOUT = 1000; 

class NDIFinder
{
	NDIlib_find_instance_t finder{};
public:
	NDIFinder()
	{
		NDIlib_find_create_t findCfg{};
		finder = NDIlib_find_create_v2(&findCfg);
		if (!finder)//nullptr in case of failure.
			throw modFatalErr("Failed to create NDI finder !");
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
			throw modFatalErr("Failed to create NDI finder !");
	}
   ~NDIFinder(){ NDIlib_find_destroy(finder); }
    auto findSrc(std::uint32_t NDI_TIMEOUT) -> std::vector<NDIlib_source_t>
	{
		const NDIlib_source_t* sources	    = nullptr;
				std::uint32_t  sourceNumber = 0;
						 bool  found	    = false;

		//try to find sources
		while (!found)
		{
			NDIlib_find_wait_for_sources(finder,NDI_TIMEOUT);
			sources = NDIlib_find_get_current_sources(finder, &sourceNumber);
			found   = true;
		}
		if (sourceNumber == 0)
			throw modFatalErr("No source found.");
		else
		{
			std::vector<NDIlib_source_t> sourcesFound(sources, sources + sourceNumber);
			return sourcesFound;
		}
	}
};

class NDIReceiver
{
	NDIlib_recv_instance_t receiver{};
public:
	NDIReceiver(const NDIlib_source_t& src)
	{
		NDIlib_recv_create_v3_t recvConfig{};
		recvConfig.p_ndi_recv_name		= src.p_ndi_name;
		recvConfig.source_to_connect_to = src;
		receiver = NDIlib_recv_create_v3(&recvConfig);
		if(!receiver)
			throw modFatalErr("Failed to create NDI receiver !");
	}
	template <audio_type T>
	void extractAudio(audio_queue<T>& audio)
	{
		NDIlib_audio_frame_v2_t audioInput {};
		if (NDIlib_recv_capture_v2(receiver, nullptr, &audioInput, nullptr, 0) == NDIlib_frame_type_audio)
		{
			constexpr bool is32f { std::is_same_v<T, float> };
			using frameType = std::conditional<is32f, 
											   NDIlib_audio_frame_interleaved_32f_t, 
											   NDIlib_audio_frame_interleaved_16s_t>::type;

			const auto dataSize = audioInput.no_samples * audioInput.no_channels;
			frameType audioFrame {};
			audioFrame.p_data = new T[dataSize];

			if constexpr (is32f)
				NDIlib_util_audio_to_interleaved_32f_v2(&audioInput, &audioFrame);
			else 
				NDIlib_util_audio_to_interleaved_16s_v2(&audioInput, &audioFrame);

			
			audio.push(std::vector<T>(audioFrame.p_data, audioFrame.p_data + dataSize),
					   audioFrame.sample_rate, 
					   audioFrame.no_channels);

			delete[] audioFrame.p_data;
		}
	}
};

class NDI : public audioMixerModule_base
{
	std::vector<NDIReceiver> recvList;
public:
	NDI(const audio_output_context& outputCfg);

    void start() override;
    void stop () override;

	void srcSearch();
	void recvAudio();
};

#pragma region IMPL NDI
/**
 * @brief Initialize NDI input module.
 * 
 * @param outputCfg ouput parameters 
 */
	 NDI::NDI	   (const audio_output_context& outputCfg)
	:	module(outputCfg) {}

/**
 * @brief Start NDI input module.
 */
void NDI::start	   ()
{
	NDIlib_initialize();
	std::print("NDI Module is activated.\n");
	active = true;
	
	try
	{
		this->srcSearch();
		this->recvAudio();
	}
	catch (const modFatalErr& fatalErr)
	{
		std::print("{}\n", fatalErr.what());
		std::print("Q(uit)  R(estart)\n");
		char ch{};
		std::cin >> ch;
		switch (std::toupper(ch))
		{
		case 'Q':
			stop();
			break;
		case 'R':
			stop();
			start();
			break;
		default:
			std::print("invalide choice!\n");
		}
		start();
	}
}
/**
 * @brief Stop NDI input module.
 */
void NDI::stop	   ()
{
	NDIlib_destroy();
	std::print("NDI Module is stopped.\n");
	active = false;
}
/**
 * @brief Search all NDI source available. 
 */
void NDI::srcSearch()
{
	try
	{
		if (!active)
			throw modFatalErr("NDI lib is not running !");

		NDIFinder audioFinder{};
		auto sourcesFound = audioFinder.findSrc(NDI_TIMEOUT);

		//print all sources found
		std::print("NDI sources :\n");
		for (auto& i : sourcesFound)
			std::print("Name : {}	\nIP   : {}\n\n",
				        i.p_ndi_name, i.p_url_address);

		//let user select NDI sources that they want.
		bool sourceMatched = false;
		bool selectAll = false;
		std::print("Please enter the IP of the source that you want to connect to.\nA(ll)	E(nd)\n");
		do
		{
			sourceMatched = false;

			std::string url;
			std::cin >> url;

			if (url == "A" || url == "a")
				selectAll = true;

			for (auto& i : sourcesFound)
			{
				if (url == i.p_url_address || selectAll)
				{
					recvList.push_back(NDIReceiver(i));
					std::print("{} selected.\n", i.p_ndi_name);
					sourceMatched = true;
				}
			}
			if (url == "E" || url == "e" || selectAll)
			{
				std::print("Sources confimed.\n");
				break;
			}
			if (!sourceMatched) std::print("No source matched! Please try again.\n");
		} while (true);
		//create a audio_queue for each selected NDI receiver.
		audio->resize(recvList.size(), audio_queue<float>(output_context));
	}
	catch (const modFatalErr& fatalErr)
	{
		std::print("NDI fatal error : {}\n", fatalErr.what());
		throw fatalErr;
	}
	catch (const modObjNotFound& nfErr)
	{
		std::print("NDI error : {}.\n", nfErr.what());
		std::print("Press any key to try again.\n");
		std::cin.ignore();
		this->srcSearch();
	}
}
/**
 * @brief Receive audio from all NDI source selected.
 */
void NDI::recvAudio()
{
	while (true)
		for (auto&& [receiver,audio] : std::views::zip(recvList, (*audio)))
			receiver.extractAudio<float>(audio);
}
#pragma endregion
//IMPL NDI

#endif//NDI_RECV_MODULE_H