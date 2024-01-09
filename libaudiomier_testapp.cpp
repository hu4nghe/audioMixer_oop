#include <print>
#include <iostream>

#include <iostream>
#include <vector>
#include <ranges>

#include "audioMixer.h"

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

int main()
{
	
	auto config = configuration();

	audioMixer mixer(config);
	mixer.startStream();
}	
	
	/*QTFF file parser test
	/*
	* auto path = "C:\Users\Modulo\Desktop\Nouveau_dossier\Music\TRAIN.mov";
	QTFF parser(filepath, config);
	parser.searchAudioInfo();
	
	soundFile file(config);
	file.start();
	return 0;



/*
int main() {
	std::vector<int> numbers = { 1, 2, 3, 4, 5 };
	std::vector<char> letters = { 'a', 'b', 'c', 'd', 'e', 'f', 'g' };

	
	auto numbersRange = numbers;

	
	auto lastFiveLettersRange = letters | std::views::take(5) ;


	auto zippedView = std::views::zip(numbersRange, lastFiveLettersRange);


	for (const auto&& [num,let] : zippedView)
	{
		num += 1;

		
		std::cout << "Number: " << num << ", Letter: " << let << std::endl;
	}

	return 0;
	return 0;
}*/