#include "audio_types.h"

inline float to_32f(const short in)
{
	return static_cast<float>(in) / 0x8000;
}

inline short to_16s(const float in)
{
	float scaled_value = in * 32768.f;
	//Range check to avoid float input (-inf;-1) union (1;inf)
	if (scaled_value >= 32767.f)
		return 32767;
	else if (scaled_value <= -32768.f)
		return -32768;
	else
		return static_cast<short>(scaled_value);
}
