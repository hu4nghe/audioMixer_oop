#include <print>

#include "audioMixer.h"

/*
* function for output configurations : sample rate, channel number, buffersize(in second)
*/
outputParameter configuration()
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

int main()
{
	auto config = configuration();
	audioMixer m1(config);
	m1.startStream();
	return 0;
}