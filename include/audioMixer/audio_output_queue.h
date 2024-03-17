#pragma once

#include <atomic>
#include <vector>

#include "audio_output_context.h"
#include "audio_types.h"

/**
 * @brief Lock-free queue for PCM audio data
 */
template <audio_type T>
class audio_queue
{
private:
    std::atomic<std::size_t> head;
    std::atomic<std::size_t> tail;
    std::atomic<std::size_t> count;
    std::vector<     T     > queue;

    audio_output_context output_context;
public:
    /* Constructors */

    /**
     * @brief Default constructor.
     */
    audio_queue();

    /**
     * @brief Construct a new audio_queue<T>::audio_queue object with following arguments :
     *
     * @tparam T             audio data type.
     * @param output_context a audio_output_context object that indicate the output parameters
     */
    audio_queue(audio_output_context output_context);

    /**
     * @brief copy constructor
     * @param another audio_queue object
     */
    audio_queue(const audio_queue<T>& other);

   
    /**
     * @brief move constructor
     * @param another audio_queue object
     */
    audio_queue(      audio_queue<T>&& other) noexcept;

    //push and pop a slice of audio sample.
    /**
     * @brief To push a numbers of samples into the buffer queue.
     *
     * @param  data                 input audio array.
     * @param  size                 input array size.
     * @param  input_sample_rate    input samplerate.       
     * @param  input_channel_number input Channel numbers. 
     * @throws std::out_of_range    in case of lack of space in the queue.
     */
    void push(      T*            data,
              const std::size_t   size,
              const std::uint32_t input_sample_rate,
              const std::uint8_t  input_channel_number);

    /**
     * @brief To pop a number of samples out of buffer queue
     *
     * @param  data   the target array.
     * @param  frames numbers of frame(not sample) to pop.
     * @throws std::out_of_range in case of lack of samples available in the queue.
     */
    void pop (      T*&         data,
              const std::size_t frames);
private:
    
    /**
     * @brief enqueue operation, push a value into the audio queue.
     *
     * use release - aquire model to ensure a sychornized-with relation between differnt therads.
     *
     * @param  value sample to push.
     * @return true  if operation successed.
     * @return false if operation failed due to out of range.
     */
    [[nodiscard]] bool enqueue(const T value);

    /**
     * @brief dequeue operation, pop a value from the queue.
     *
     * use release - aquire model to ensure a sychornized-with relation between differnt therad.
     *
     * @param value  the target variable for output.
     * @return true  if operation successed.
     * @return false if operation failed due to lack of samples in the queue.
     */
    [[nodiscard]] bool dequeue(T& value);

    //abandon all samples and clear the queue
    void clear();

    /**
     * @brief In the necessary case : 
              resample a slice of audio using libsamplerate, from "inputSamplerate" to output sample rate.
     *        convert the number of audio channels to the expected output channel count.
     *
     * @param data                 input vector
     * @param input_sample_rate    input sample rate.
     * @param input_channel_number input channel numbers.
     * @return a vector containing the samples after resampling
     */
    std::vector<T> input_process(      T*            data,
                                 const std::size_t   size,
                                 const std::uint32_t input_sample_rate,
                                 const std::uint8_t  input_channel_number);
    
public:
    //getters
    [[nodiscard]] inline auto empty         () const noexcept { return !count.load();                 }
    [[nodiscard]] inline auto size          () const noexcept { return count.load();                  }
    [[nodiscard]] inline auto capacity      () const noexcept { return output_context.capacity;       }
    [[nodiscard]] inline auto channel_number() const noexcept { return output_context.channel_number; }
    [[nodiscard]] inline auto sample_rate   () const noexcept { return output_context.sample_rate;    }

    /**
     * @brief set a new output context for audio queue
     * warning : this operation will abandon all current samples in the queue.
     *
     * @param new_context new output context.     
     * @return a vector containing the samples after resampling
     */
    inline void set_output_context(const audio_output_context& new_context);
};