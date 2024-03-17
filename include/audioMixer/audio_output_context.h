#pragma once

#include <cstdint>
#include <algorithm>

constexpr std::size_t default_sample_rate = 44100;
constexpr std::size_t default_channel_num = 2;
constexpr std::size_t default_buffer_size = 441000;//1 seconds
constexpr std::size_t default_min_buffer  = 0;

/**
 * @brief a struct that declares :
 *  - audio ouput configuration.
 *  - max and min buffer size for audio mixer.
 */
struct audio_output_context
{
    /* output sample rate */
    std::size_t  sample_rate     = default_sample_rate;

    /* output channel number */
    std::uint8_t channel_number  = default_channel_num;

    /* capacity of audio_queue */
    std::size_t  capacity        = default_buffer_size;

    std::size_t  minimum_element = default_min_buffer ;
};

