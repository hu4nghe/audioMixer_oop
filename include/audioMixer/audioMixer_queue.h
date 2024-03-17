#ifndef AUDIOMIXER_QUEUE_H
#define AUDIOMIXER_QUEUE_H

#include <atomic>
#include <concepts>
#include <cstdint>
#include <vector>

#include "audio_output_context.h"
#include "samplerate.h"


/**
 * @brief audio sample data type.
 * @tparam T audio data type : 32bit float[-1;1], 16bit short[-32768;32767].
 */

//32 bit float audio type
template<typename T>
concept audio_float32 = 
    //32bit float type
    std::floating_point<T> && 
    //in interval [-1;1]
    requires(T value)
    {
        { value >= -1.0f } -> std::convertible_to<bool>;
        { value <=  1.0f } -> std::convertible_to<bool>;
    };

template<typename T>
concept audio_short16 =
    //16 bit short type
    std::same_as<T, std::int16_t>;
    
template<typename T>
concept audio_t = std::same_as<T, std::int16_t>; || std::same_as<T, float>;

/**
 * @brief Lock-free queue for PCM audio data 
 * (resampling input audio to output sample rate by : libsamplerate.)
 */
template <audio_t T>
class audio_queue 
{
private :
    std::atomic<std::  size_t> head;
    std::atomic<std::  size_t> tail;                
    std::atomic<std::  size_t> count;
                std::vector<T> queue;

    audio_output_context output_context;                                   
public : 
    audio_queue();
    audio_queue(       audio_output_context output_context);
    audio_queue(const  audio_queue<T>&  other);
    audio_queue(       audio_queue<T>&& other) noexcept;
    bool push (      std::vector<T>&& data,
               const std::uint32_t    inputSampleRate,
               const std:: uint8_t    inputChannelNumber);
    bool pop  (                  T*&  data,
               const std::  size_t    frames,
               const          bool    mode);
private :
                  void clear  ();
    [[nodiscard]] bool enqueue(const    T   value);
    [[nodiscard]] bool dequeue(         T&  value,
                               const bool   mode);
    //Audio processing
    void resample      (       std::vector<T>& data,
                        const  std::uint32_t   inputSampleRate,
                        const  std:: uint8_t   inputChannelNumber);
    void channelConvert(       std::vector<T>& data,
                        const  std:: uint8_t   originalChannelNumber);
public :
    //getters
    [[nodiscard]] inline          bool empty     () const noexcept { return !count.load(); }
    [[nodiscard]] inline std::  size_t size      () const noexcept { return  count.load(); }
    [[nodiscard]] inline std::  size_t capacity  () const noexcept { return  queue.size(); }
    [[nodiscard]] inline std:: uint8_t channels  () const noexcept { return  output_context.channelNumber;  }
    [[nodiscard]] inline std::uint32_t sample_rate() const noexcept { return  output_context.sample_rate;     }
    [[nodiscard]] inline std::  size_t minElement() const noexcept { return  output_context.minimumElement; }
    //setters
    inline void setSampleRate(const std::uint32_t newSampleRate)   { output_context.sample_rate     = newSampleRate;    }
    inline void setChannelNum(const std:: uint8_t newChannelNumber){ output_context.channelNumber  = newChannelNumber; }
    inline void setMinBuffer (const std::  size_t newMinBuffer)    { output_context.minimumElement = newMinBuffer;     }
           void setCapacity  (const std::  size_t newCapacity);
};


#endif// AUDIO_QUEUE_H