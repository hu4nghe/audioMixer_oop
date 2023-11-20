#ifndef AUDIOMIXER_IMPL_H
#define AUDIOMIXER_IMPL_H

#include <unordered_map>

#include "audioMixer_NDI_Recv.h"
#include "audioMixer_file.h"
#include "audioMixer_DeltaCast.h"
#include "portaudio.h"

#define PA_CALLBACK_PARAM_LIST const					 void*	inputBuffer,	\
														 void*	outputBuffer,	\
												unsigned long	framesPerBuffer,\
							   const PaStreamCallbackTimeInfo*	timeInfo,		\
										PaStreamCallbackFlags	statusFlags

class audioMixer
{
	friend class portaudio;

	/*Configs*/
	outputParameter outputConfig;
	/*Input modules*/
	std::unordered_map<std::string,std::unique_ptr<module>> inputModules;

	/*portaudio*/
	  PaStream* PaStreamOut;
	static int  PaCallbackTransfer(PA_CALLBACK_PARAM_LIST, void* userData);
		   int  PaOutCallback	  (PA_CALLBACK_PARAM_LIST);
		  void  PaErrorCheck	  (const PaError& err);
		  void  PaStart			  ();
		  void  PaStop			  ();

public:
    audioMixer(const outputParameter& outputCfg);
	void startStream();
};

#pragma region IMPL audioMixer

audioMixer::audioMixer(const outputParameter& outputCfg)
	:	outputConfig (outputCfg)
{
	/*input modules init function*/
	auto NDIInit	 = [&]()
	{ inputModules.insert(std::make_pair <std::string, std::unique_ptr<NDI>>
										 ("NDI"		 , std::make_unique<NDI>(outputCfg))); };
	auto sndfileInit = [&]()
	{ inputModules.insert(std::make_pair <std::string, std::unique_ptr<soundFile>>
										 ("sndifle"	 , std::make_unique<soundFile>(outputCfg))); };
	auto deltaCastInit = [&]()
	{ inputModules.insert(std::make_pair <std::string, std::unique_ptr<deltaCast>>
										 ("deltaCast", std::make_unique<deltaCast>(outputCfg))); };
	/*Module select*/
	char ch{};
	std::pair<bool, std::unique_ptr<module>> mod{};

	std::print("Module selection :\n");
	std::print("N(DI)	S(ndfile)	D(eltaCast)		A(ll)\n");

	std::cin >> ch;

	if (ch == 'A' || ch == 'a')
	{
		NDIInit();
		sndfileInit();
		deltaCastInit();
	}
	else if (ch == 'N' || ch == 'n')
		NDIInit();
	else if (ch == 'S' || ch == 's')
		sndfileInit();
	else if (ch == 'D' || ch == 'd')
		deltaCastInit();

	system("cls");

	/*portaudio init function*/
	PaErrorCheck(Pa_Initialize());
	PaErrorCheck(Pa_OpenDefaultStream(&PaStreamOut,
									   0,							//output only
									   outputConfig.channelNumber,
									   paFloat32,					//32 bit float [-1;1]
								       outputConfig.sampleRate,
									   64,							//frames per buffer
									  &audioMixer::PaCallbackTransfer,
									   this));
}

inline void audioMixer::startStream()
{
	std::jthread pThread(&audioMixer::PaStart,this);
	for (auto& [key,instance] : inputModules)
		instance->start();
}

#pragma region portaudio module

int audioMixer::PaCallbackTransfer(PA_CALLBACK_PARAM_LIST, void* userData)
{
	return static_cast<audioMixer*>(userData)->PaOutCallback(inputBuffer,
															 outputBuffer,
															 framesPerBuffer,
															 timeInfo,
															 statusFlags);
}

int audioMixer::PaOutCallback(PA_CALLBACK_PARAM_LIST)
{
	//output buffer init
	auto out = static_cast<float*>(outputBuffer);
	memset(out, 0, sizeof(out) * framesPerBuffer);

	for (auto& [name, moduleInstance] : inputModules)
		if (moduleInstance->isActive())
			for (auto& audioVec : *(moduleInstance->getAudio()))
				audioVec.pop(out, framesPerBuffer, true);

	return paContinue;
}

void audioMixer::PaErrorCheck(const PaError& err)
{
	if (err)
	{
		std::print("PortAudio error : {}.\n",
			Pa_GetErrorText(err));
		exit(EXIT_FAILURE);
	}
}

inline void audioMixer::PaStart()
{
	std::this_thread::sleep_for(std::chrono::seconds(10));
	Pa_StartStream(PaStreamOut);
}

inline void audioMixer::PaStop()
{
	if (Pa_IsStreamActive(PaStreamOut))
		PaErrorCheck(Pa_StopStream(PaStreamOut));
}
#pragma endregion 
//portaudio module

#pragma endregion
//IMPL audioMixer

#endif//AUDIOMIXER_IMPL_H