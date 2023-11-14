#ifndef AUDIOMIXER_PORTAUDIO_H
#define AUDIOMIXER_PORTAUDIO_H

#include <print>

#include "audioMixer_base.h"
#include "portaudio.h"

#define PA_CALLBACK_PARAM_LIST const					 void*	inputBuffer,	\
														 void*	outputBuffer,	\
												unsigned long	framesPerBuffer,\
							   const PaStreamCallbackTimeInfo*	timeInfo,		\
										PaStreamCallbackFlags	statusFlags

class audioMixer;

class portaudio : public module
{
public:
	static  int PaCallbackTransfer(PA_CALLBACK_PARAM_LIST, void* userData);
		    int PaOutCallback	  (PA_CALLBACK_PARAM_LIST);
		   void PaErrorCheck	  (const PaError& err);
private:
	const audioMixer* currentMixer;
		    PaStream* PaStreamOut;
	

public:
	portaudio(const outputParameter& outputCfg,const audioMixer* currMixer);
   ~portaudio();

   void start() override;
   void stop()	override;


};
#pragma region IMPL

int portaudio::PaCallbackTransfer(PA_CALLBACK_PARAM_LIST, void* userData)
{
	return static_cast<portaudio*>(userData)->PaOutCallback(inputBuffer,
															outputBuffer,
															framesPerBuffer,
															timeInfo,
															statusFlags);
}

int portaudio::PaOutCallback(PA_CALLBACK_PARAM_LIST)
{
	//output buffer init
	auto out = static_cast<float*>(outputBuffer);
	memset(out, 0, sizeof(out) * framesPerBuffer);

	for (auto& [name,moduleInstance] : currentMixer->inputModules)
		if (moduleInstance->isActive())
			for (auto& audioVec : *(moduleInstance->getAudio()))
				audioVec.pop(out, framesPerBuffer, true);

	return paContinue;
}

void portaudio::PaErrorCheck(const PaError& err)
{
	if (err)
	{
		std::print("PortAudio error : {}.\n", 
					Pa_GetErrorText(err));
		exit(EXIT_FAILURE);
	}
}

inline portaudio::portaudio(const outputParameter& outputCfg, const audioMixer* currMixer)
	:	module	    (outputCfg),
		PaStreamOut (nullptr),
		currentMixer(currMixer)
{
	PaErrorCheck(Pa_Initialize());
	PaErrorCheck(Pa_OpenDefaultStream(&PaStreamOut,
									   0,							//output only
									   outputConfig.channelNumber,
									   paFloat32,					//32 bit float [-1;1]
									   outputConfig.sampleRate,
									   128,							//frames per buffer
									  &portaudio::PaCallbackTransfer,
									   this));
}

inline portaudio::~portaudio()
{
	if(Pa_IsStreamActive(PaStreamOut))
		PaErrorCheck(Pa_StopStream(PaStreamOut));

	PaErrorCheck(Pa_CloseStream(PaStreamOut));
	PaErrorCheck(Pa_Terminate());
}

inline void portaudio::start()
{
	if (Pa_IsStreamStopped(PaStreamOut))
		Pa_StartStream(PaStreamOut);
}

inline void portaudio::stop()
{
	if (Pa_IsStreamActive(PaStreamOut))
		PaErrorCheck(Pa_StopStream(PaStreamOut));
}

#pragma endregion
#endif//AUDIOMIXER_PORTAUDIO_H