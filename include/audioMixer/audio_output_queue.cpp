#pragma once

#include <xutility>
#include <memory>

#include "audio_output_queue.h"
#include "audio_exception.h"
#include "samplerate.h"

//default constructor
template<audio_type T>
audio_queue<T>::audio_queue()
    : queue(0),
      head(0),
      tail(0),
      count(0) {}

//initialize the queue with a output context
template<audio_type T>
audio_queue<T>::audio_queue(audio_output_context output_context)
    : queue(output_context.capacity),
      head(0),
      tail(0),
      count(0),
      output_context(output_context) {}

//copy constructor
template<audio_type T>
audio_queue<T>::audio_queue(const audio_queue<T>& other)
    : queue(other.queue),
      head(other.head.load()),
      tail(other.tail.load()),
      count(other.count.load()),
      output_context(other.output_context) {}

//move constructor
template<audio_type T>
audio_queue<T>::audio_queue(audio_queue<T>&& other) noexcept
    : queue(std::move(other.queue)),
      head(other.head.load()),
      tail(other.tail.load()),
      count(other.count.load()),
      output_context(other.output_context) {}

//enqueue
template<audio_type T>
bool audio_queue<T>::enqueue(const T value)
{
    auto currTail = tail.load(std::memory_order_relaxed);
    auto nextTail = (currTail + 1) % queue.size();
    if (nextTail == head.load(std::memory_order_acquire))
        return false; // Queue is full
    queue[currTail] = value;
    tail.store(nextTail, std::memory_order_release);
    count.fetch_add(1, std::memory_order_relaxed);
    return true;
}

//dequeue
template<audio_type T>
bool audio_queue<T>::dequeue(T& value)
{
    auto currHead = head.load(std::memory_order_relaxed);
    auto nextHead = (currHead + 1) % queue.size();
    if (currHead == tail.load(std::memory_order_acquire))
        return false; // Queue is empty
    value += queue[currHead];//addition mode.
    head.store(nextHead, std::memory_order_release);
    count.fetch_sub(1, std::memory_order_relaxed);
    return true;
}

//purge all samples and clear the queue
template<audio_type T>
void audio_queue<T>::clear()
{
    head.store(0);
    tail.store(0);
    count.store(0);
}

//resample by libsamplerate
template<audio_type T>
std::vector<T> audio_queue<T>::input_process(      T*            data,
                                             const std::size_t   size,
                                             const std::uint32_t input_sample_rate,
                                             const std::uint8_t  input_channel_number)
{
    const auto needs_resample        = input_sample_rate    != output_context.sample_rate;
    const auto needs_channel_mapping = input_channel_number != output_context.channel_number;
    const auto is_16s = std::is_same<T, short>::value;

    //nothing to do
    if (!(needs_resample || needs_channel_mapping))
        //nothing to process.
        return std::vector<T>(data, data + size);

    std::unique_ptr<std::vector<T>> p_source_32f;
    std::unique_ptr<std::vector<T>> p_result_16s;

    if (is_16s)
    {
        p_source_32f = std::make_unique<std::vector<T>>(size);
        for (std::size_t i = 0; i < size; i++)
            (*p_source_32f)[i] = to_32f(data[i]);
    }
    std::vector<T> resample_result;

    if(needs_resample)
    {
        
        const auto resample_ratio = static_cast<double>(output_context.sample_rate) / input_sample_rate;
        const long input_frames   = static_cast<long>(size / input_channel_number);
        const long output_frames  = static_cast<long>(input_frames * resample_ratio);
        const auto new_size       = static_cast<std::size_t>(output_frames * input_channel_number);

        std::unique_ptr<std::vector<T>> p_source_data;
        //map 16s to 32f for resampling.
        if (is_16s)
        {
            
        }
                
        //resample using libsamplerate.
        resample_result.resize(new_size);

        SRC_STATE* src_state = src_new(SRC_SINC_BEST_QUALITY, 
                                       input_channel_number, 
                                       nullptr);
        SRC_DATA src_data {};
        src_data.end_of_input  = true;
        if(is_16s)
            src_data.data_in   = p_source_data -> data();
        else
            src_data.data_in   = data;
        src_data.data_out      = resample_result.data();
        src_data.input_frames  = input_frames;
        src_data.output_frames = output_frames;
        src_data.src_ratio     = resample_ratio;

        src_process(src_state, &src_data);
        src_delete(src_state);

        
        if (is_16s)
            for (auto& sample : resample_result)
                sample = to_16s(sample);
        
        if (!needs_channel_mapping)
            return resample_result;
    }
    if (needs_channel_mapping)
    {
        std::unique_ptr<std::vector<T>> p_source_data;
        if (needs_resample)
            p_source_data = std::make_unique<std::vector<T>>(resample_result);
        else
            p_source_data = std::make_unique<std::vector<T>>(data, data + size);
        
        const auto frames               = p_source_data->size() / input_channel_number;
        const auto new_size             = frames * output_context.channel_number;
        const auto valid_channel_number = std::min(input_channel_number, output_context.channel_number);


        std::vector<T> converted_data(new_size);

        for (std::size_t i = 0; i < frames; i++)
            for (std::uint8_t j = 0; j < valid_channel_number; j++)
                converted_data[i * output_context.channel_number + j] = (*p_source_data)[i * input_channel_number + j];
        
        return converted_data;
    }
}

template<audio_type T>
void audio_queue<T>::push(      T*            data, 
                          const std::size_t   size, 
                          const std::uint32_t input_sample_rate, 
                          const std::uint8_t  input_channel_number)
{
    auto processed_data = input_process(data, size, input_sample_rate, input_channel_number);
    for (auto& sample : processed_data)
        if (!enqueue(sample))
            throw module_object_out_of_range("No enough space in queue.");

    return;
}

template<audio_type T>
void audio_queue<T>::pop(      T*&         data,
                         const std::size_t frames)
{
    for (std::size_t i = 0; i < frames * output_context.channel_number; i++)
        if (!dequeue(data[i]))
            throw module_object_out_of_range("No enough sample in queue.");
    return;
}

template<audio_type T>
inline void audio_queue<T>::set_output_context(const audio_output_context& new_context)
{
    clear();
    output_context = new_context;
}