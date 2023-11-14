#ifndef AUDIOMIXER_IMPL_H
#define AUDIOMIXER_IMPL_H


#include <thread>
#include <unordered_map>
#include <utility>

#include "audioMixer_NDI_Recv.h"
#include "audioMixer_file.h"
#include "audioMixer_portaudio.h"
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
	/*
	* order : 
	* [0] NDI
	* [1] sndfile
	* [2] 
	*/
	std::unordered_map<std::string,std::unique_ptr<module>> inputModules;
	portaudio PaOuputModule;
public:
    audioMixer(const outputParameter& outputCfg);
   ~audioMixer();

    void NDIThread();

    void startStream();
	void stopStream ();
};

#pragma region IMPL

audioMixer::audioMixer(const outputParameter& outputCfg)
{
	outputConfig = outputCfg;

	char ch{};
	std::pair<bool, std::unique_ptr<module>> mod{};

	std::print("Module selection :\n");
	std::print("N(DI)	S(ndfile)	A(ll)\n");

	std::cin >> ch;

	if (ch == 'A' || ch == 'a')
	{
		mod.first
	}
	else if (ch == 'N' || ch == 'n')
	{

	}
	else if (ch == 'S' || ch == 's')
		fileEnabled = true;

	system("cls");

	
	
	//Enable NDI
	if (NDIEnabled)
	{
		if (!NDI)
		{
			NDI = std::make_unique<NDI>(outConfig);
			NDI->start();
		}
	}
	else NDI = nullptr;

	//Enable libsndfile
	if (fileEnabled)
	{
		if (!file)
		{
			file = std::make_unique<soundFile>(outConfig);
			file->start();
		}
	}
	else file = nullptr;
	Pa_StartStream(PaStreamOut);
}

audioMixer::~audioMixer()
{
	if (NDI)
	{
		NDI->stop();
		NDI.release();
	}
	if (file)
	{
		file->stop();
		file.release();
	}

	PaErrorCheck(Pa_StopStream(PaStreamOut));
	PaErrorCheck(Pa_CloseStream(PaStreamOut));
	PaErrorCheck(Pa_Terminate());
}

void audioMixer::NDIThread()
{
	NDI->sourceSearch();
	NDI->recvAudio();
}

void audioMixer::startStream()
{
	if(NDIEnabled)
		std::jthread tNDI(&audioMixer::NDIThread, this);
}

void audioMixer::stopStream()
{
	NDI->stop();
}

#pragma endregion
#endif// AUDIOMIXER_PORTAUDIO_H