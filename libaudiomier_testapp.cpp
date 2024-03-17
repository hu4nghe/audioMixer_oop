#include <print>
#include "audio_output_queue.h"
#include "audio_output_queue.cpp"
#include "audio_types.cpp"

/*
* function for output configurations : sample rate, channel number, buffersize(in second)

static audio_output_context configuration()
{
	audio_output_context output_context {};
			   char ch		  {};

	std::print("Audio mixer V1\n");
	std::print("Ouput parameters configuration :\n");
	std::print("Do you want to use default parameters?\nY(es)	N(o)\n");
	std::cin >> ch;
	if (ch == 'Y' || ch == 'y')
	{
		audio_output_context out;
		output_context = out;
	}
	else if (ch == 'N' || ch == 'n')
	{
		std::print("Please enter the ouput sample rate : ");
		std::cin >> output_context.sample_rate;

		int cNum;
		std::print("Please enter the number of ouput channels : ");
		std::cin >> cNum;

		output_context.channelNumber = static_cast<uint8_t>(cNum);

		std::size_t time;
		std::print("Please enter the maximum buffer size(in second) : ");
		std::cin >> time;
		output_context.queueCapacity = time * output_context.sample_rate * output_context.channelNumber;
		std::print("Please enter the minimum buffer size(in second) : ");
		std::cin >> time;
		output_context.minimumElement = time * output_context.sample_rate * output_context.channelNumber;
	}
	return output_context;
}*/

int main()
{
	float data[] = {9,1,2,3,5,6,2,2,1,3,5,6,7,2,5,3};
	audio_output_context ctx;
	ctx.channel_number = 8;
	audio_queue<float> queue(ctx);
	queue.push(data, 16, 44100, 2);
	float* out = new float[128];
	memset(out, 0, 128 * sizeof(float));
	queue.pop(out, 8);
	for (int i = 0; i < 64; i+=2)
		std::print("{} {}\n", out[i], out[i + 1]);
}	
	
	/*QTFF file parser test
	/*
	* auto path = "C:\\Users\Modulo\Desktop\Nouveau_dossier\Music\TRAIN.mov";
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