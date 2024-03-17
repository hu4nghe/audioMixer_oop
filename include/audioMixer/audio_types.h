#pragma once

#include <concepts>
#include <cstdint>

/**
 * @brief audio sample data type.
 */

//32 bit float audio type.
template<typename T>
concept audio_32f = std::same_as<T, float>;

//16 bit short audio type.
template<typename T>
concept audio_16s = std::same_as<T, std::int16_t>;

//ensemble of two audio type.
template<typename T>
concept audio_type = audio_32f<T> || audio_16s<T>;


//Convert a sample from 16 bit short to 32 bit float
inline float to_32f(const short in);

//Convert a sample from 32 bit float to 16 bit short
inline short to_16s(const float in);