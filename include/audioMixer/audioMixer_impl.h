#ifndef AUDIOMIXER_IMPL_H
#define AUDIOMIXER_IMPL_H

#include "portaudio.h"

#include <unordered_map>
#include <typeinfo>
#include <thread>

#include "audioMixer_NDI_Recv.h"
#include "audioMixer_file.h"
#include "audioMixer_DeltaCast.h"


#define PA_CALLBACK_PARAM_LIST const					 void*	inputBuffer,	\
														 void*	outputBuffer,	\
												unsigned long	framesPerBuffer,\
							   const PaStreamCallbackTimeInfo*	timeInfo,		\
										PaStreamCallbackFlags	statusFlags
/**
 * @brief
 *
 * @tparam T input module type : all derivated class of audioMixerModule_base, aka module.
 */
template<typename U>
concept inputMod_t = std::is_base_of<module, U>::value;

class audioMixer
{
	/*Input modules*/
	std::unordered_map<std::string,std::unique_ptr<module>> inputModules;
	/*Portaudio output configurations*/
	outputParameter outputConfig;
	/*portaudio stream ptr*/
	PaStream* PaStreamOut;
	/*portaudio functions*/
	static int  PaCallbackTransfer(PA_CALLBACK_PARAM_LIST, void* userData);
	int  PaOutCallback(PA_CALLBACK_PARAM_LIST);
	void PaErrorCheck (const PaError& err);
	void PaStart	  ();
	void PaStop		  ();
	/*input modules init function*/
	template<inputMod_t U>
	void inputModuleInit();
public:
    audioMixer(const outputParameter& outputCfg);
	void startStream();
};

#pragma region IMPL audioMixer
#pragma region Private functions
template<inputMod_t U>
inline void audioMixer::inputModuleInit()
{
	auto typeName(typeid(U).name());
	if (!inputModules.contains(typeName))
	{
		inputModules.insert(std::make_pair <std::string, std::unique_ptr <U>>
			(typeName, std::make_unique<U>(outputConfig)));
		std::print("{} module is enabled.\n", typeName);
	}
	else
		std::print("Module already exist, please try another option.\n");
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
inline void audioMixer::PaErrorCheck(const PaError& err)
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
//Private functions

audioMixer::audioMixer(const outputParameter& outputCfg)
	:	outputConfig (outputCfg)
{
	/*Module select*/
	char ch{};
	bool moduleSelected{ false };
	std::print("Module selection :\n");
	std::print("N(DI)	S(ndfile)	D(elta Cast)	A(ll)	E(nd)\n");
	do
	{
		std::cin >> ch;
		if (ch == 'A' || ch == 'a')
		{
			inputModuleInit<NDI		 >();
			inputModuleInit<soundFile>();
			inputModuleInit<deltaCast>(); 
			moduleSelected = true;
		}
		else if (ch == 'N' || ch == 'n')
			inputModuleInit<NDI		 >();
		else if (ch == 'S' || ch == 's')
			inputModuleInit<soundFile>();
		else if (ch == 'D' || ch == 'd')
			inputModuleInit<deltaCast>();
		else if (ch == 'E' || ch == 'e')
			moduleSelected = true;
		else
			std::print("invalide choice!\n");
	} while (!moduleSelected);
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
	std::vector<std::jthread> threads;
	threads.emplace_back(&audioMixer::PaStart, this);
	for (auto& [key, instance] : inputModules) 
		threads.emplace_back([&instance](){ instance->start(); });
}

#pragma endregion
//IMPL audioMixer

#endif//AUDIOMIXER_IMPL_H
