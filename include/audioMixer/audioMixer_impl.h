#ifndef AUDIOMIXER_IMPL_H
#define AUDIOMIXER_IMPL_H

#include "portaudio.h"

#include <unordered_map>
#include <typeinfo>
#include <thread>
#include <cctype>

#include "audioMixer_NDI_Recv.h"
#include "audioMixer_file.h"
#include "audioMixer_DeltaCast.h"

//portaudio callback function parameter list
#define PA_CALLBACK_PARAM_LIST const void*						inputBuffer,	\
									 void*						outputBuffer,	\
									 unsigned long				framesPerBuffer,\
							   const PaStreamCallbackTimeInfo*	timeInfo,		\
									 PaStreamCallbackFlags		statusFlags

class audioMixer
{
	//Input modules
	std::unordered_map<std::string,std::unique_ptr<module>> inputModules;
	//Portaudio output configurations
	outputParameter outputConfig;
	//portaudio stream ptr
	PaStream* PaStreamOut;
	//portaudio functions
	static int PaCallbackTransfer(PA_CALLBACK_PARAM_LIST, void* userData);

	int  PaOutCallback(PA_CALLBACK_PARAM_LIST);
	void PaErrorCheck (const PaError& err);
	void PaStart	  ();
	void PaStop		  ();
	//input modules init function
	template<inputMod_t U>
	void inputModInit();
public:
    audioMixer(const outputParameter& outputCfg);
	void startStream();
};

#pragma region IMPL
template<inputMod_t U>
inline void audioMixer::inputModInit	  ()																
{
	auto typeName(typeid(U).name());
	if (!inputModules.contains(typeName))
	{
		inputModules.insert(std::make_pair <std::string, std::unique_ptr <U>>
										   (typeName   , std::make_unique<U>(outputConfig)));

		std::print("{} module is enabled.\n", typeName);
	}
	else
		std::print("Module already exist, please try another option.\n");
}
audioMixer::audioMixer					  (const outputParameter& outputCfg)
	: outputConfig(outputCfg)
{
	/*Module select*/
	char ch		 {};
	bool selected{ false };
	std::print("Module selection :\n");
	std::print("N(DI)	S(ndfile)	D(elta Cast)	A(ll)	E(nd)\n");
	do
	{
		std::cin >> ch;
		switch(std::toupper(ch))
		{
			case 'A':
				inputModInit<   NDI	  >();
				inputModInit<soundFile>();
				inputModInit<deltaCast>();
				selected = true;
				break;
			case 'N':
				inputModInit<   NDI	  >();
				break;
			case 'S':
				inputModInit<soundFile>();
				break;
			case 'D':
				inputModInit<deltaCast>();
				break;
			case 'E':
				selected = true;
				break;
			default :
				std::print("invalide choice!\n");
		}
	} while (!selected);
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
inline void audioMixer::startStream		  ()								
{
	std::vector<std::jthread> threads;
	threads.emplace_back(&audioMixer::PaStart, this);
	for (auto& [key, instance] : inputModules)
		threads.emplace_back([&instance]() { instance->start(); });
}
inline	int audioMixer::PaCallbackTransfer(PA_CALLBACK_PARAM_LIST, void* userData)
{
	return static_cast<audioMixer*>(userData)->PaOutCallback(inputBuffer,
		outputBuffer,
		framesPerBuffer,
		timeInfo,
		statusFlags);
}
inline  int audioMixer::PaOutCallback	  (PA_CALLBACK_PARAM_LIST)
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
inline void audioMixer::PaErrorCheck	  (const PaError& err)
{
	if (err)
		throw std::runtime_error(Pa_GetErrorText(err));
}
inline void audioMixer::PaStart			  ()
{
	if (Pa_IsStreamStopped(PaStreamOut))
		PaErrorCheck(Pa_StartStream(PaStreamOut));
}
inline void audioMixer::PaStop			  ()
{
	if (Pa_IsStreamActive(PaStreamOut))
		PaErrorCheck(Pa_StopStream(PaStreamOut));
}
#pragma endregion
//IMPL audioMixer
#endif//AUDIOMIXER_IMPL_H