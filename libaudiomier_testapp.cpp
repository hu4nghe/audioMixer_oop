#include <print>
#include <print>

#include "audioMixer.h"
#include "audioMixer_QTFF_Parser.h"

/*
* function for output configurations : sample rate, channel number, buffersize(in second)
*/
static outputParameter configuration()
{
	outputParameter outConfig {};
			   char ch		  {};

	std::print("Audio mixer V1\n");
	std::print("Ouput parameters configuration :\n");
	std::print("Do you want to use default parameters?\nY(es)	N(o)\n");
	std::cin >> ch;
	if (ch == 'Y' || ch == 'y')
	{
		outputParameter out;
		outConfig = out;
	}
	else if (ch == 'N' || ch == 'n')
	{
		std::print("Please enter the ouput sample rate : ");
		std::cin >> outConfig.sampleRate;

		int cNum;
		std::print("Please enter the number of ouput channels : ");
		std::cin >> cNum;

		outConfig.channelNumber = static_cast<uint8_t>(cNum);

		std::size_t time;
		std::print("Please enter the maximum buffer size(in second) : ");
		std::cin >> time;
		outConfig.queueCapacity = time * outConfig.sampleRate * outConfig.channelNumber;
		std::print("Please enter the minimum buffer size(in second) : ");
		std::cin >> time;
		outConfig.minimumElement = time * outConfig.sampleRate * outConfig.channelNumber;
	}
	return outConfig;
}
static bool is_big_endian(void)
{
	union {
		uint32_t i;
		char c[4]; 
	} bint = { 0x01020304 };

	return bint.c[0] == 1;
}

int main()
{
	
	auto config = configuration();

	/* audioMixer test*/
	
	audioMixer mixer(config);
	mixer.startStream();
	
	
	/*QTFF file parser test*/
	/*
	* auto path = "C:\Users\Modulo\Desktop\Nouveau_dossier\Music\TRAIN.mov";
	QTFF parser(filepath, config);
	parser.searchAudioInfo();
	
	soundFile file(config);
	file.start();*/

	return 0;
}