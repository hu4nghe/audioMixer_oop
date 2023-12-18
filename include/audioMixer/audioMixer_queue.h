#ifndef AUDIOMIXER_QUEUE_H
#define AUDIOMIXER_QUEUE_H

#include <atomic>
#include <concepts>
#include <vector>

#include "samplerate.h"

#pragma region Output parameters class

/*default values*/
constexpr std::size_t defaultSampleRate = 48000;
constexpr std::size_t defaultChannelNum = 2;
constexpr std::size_t defaultBufferSize = 480000;
constexpr std::size_t defaultMinBuffer  = 0;

struct outputParameter
{
    std:: size_t sampleRate;
    std::uint8_t channelNumber;
    std:: size_t queueCapacity;
    std:: size_t minimumElement;

    outputParameter(const std:: size_t sRate      = defaultSampleRate,
                    const std::uint8_t nbChannel  = defaultChannelNum,
                    const std:: size_t maxElement = defaultBufferSize,
                    const std:: size_t minElement = defaultMinBuffer)
        :   sampleRate    (sRate),
            channelNumber (nbChannel),
            queueCapacity (maxElement),
            minimumElement(minElement) {}
};
#pragma endregion

/**
 * @brief audio sample data type.
 *
 * @tparam T audio data type : 32bit float[-1;1], 16bit short[-32768;32767].
 */
template<typename T>
concept audio_t = std::same_as<T, std::int16_t> || std::same_as<T, float>;

template <audio_t T>
class audioQueue 
{
private :
    std::atomic<std::  size_t> head;
    std::atomic<std::  size_t> tail;                
    std::atomic<std::  size_t> count;
                std::vector<T> queue;

    outputParameter outConfig;                                   
public : 
    audioQueue();
    audioQueue(       outputParameter outputConfig);
    audioQueue(const  audioQueue<T>&  other);
    audioQueue(       audioQueue<T>&& other) noexcept;
    bool push (      std::vector<T>&  data,
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
    [[nodiscard]] inline std:: uint8_t channels  () const noexcept { return  outConfig.channelNumber;  }
    [[nodiscard]] inline std::uint32_t sampleRate() const noexcept { return  outConfig.sampleRate;     }
    [[nodiscard]] inline std::  size_t minElement() const noexcept { return  outConfig.minimumElement; }
    //setters
    inline void setSampleRate(const std::uint32_t newSampleRate)   { outConfig.sampleRate     = newSampleRate;    }
    inline void setChannelNum(const std:: uint8_t newChannelNumber){ outConfig.channelNumber  = newChannelNumber; }
    inline void setMinBuffer (const std::  size_t newMinBuffer)    { outConfig.minimumElement = newMinBuffer;     }
           void setCapacity  (const std::  size_t newCapacity);
};

#pragma region IMPL
#pragma region Constructors
/**
 * @brief Default constructor.
 */
template<audio_t T>
audioQueue<T>::audioQueue()
    :   queue(0),
        head (0), 
        tail (0), 
        count(0){}

/**
 * @brief Construct a new audioQueue<T>::audioQueue object with following arguments : 
 * 
 * @tparam T                   audio data type.
 * @param sampleRate           OUTPUT samplerate that you want.
 * @param channelNumbers       OUTPUT channels that you want.
 * @param sampleCapacity       Capacity of buffer, in sample numbers.
 * @param minimumElementNumber The queue will reject all pop requests if the number of elements is lower than this threshold.
 */
template<audio_t T>
audioQueue<T>::audioQueue(outputParameter outputConfig)
    :   queue    (outputConfig.queueCapacity),
        outConfig(outputConfig),
        head     (0), 
        tail     (0),
        count    (0){}

/**
 * @brief Copy constructor
 */
template<audio_t T>
audioQueue<T>::audioQueue(const  audioQueue<T>& other)
    :   queue    (other.queue),
        outConfig(other.outConfig),
        head     (other.head. load()),
        tail     (other.tail. load()),
        count    (other.count.load()){}

/**
 * @brief Move constructor
 */
template<audio_t T>
audioQueue<T>::audioQueue(audioQueue<T>&& other) noexcept
    :   queue    (std::move(other.queue)),
        outConfig(other.outConfig),
        head     (other.head .load()),
        tail     (other.tail .load()),
        count    (other.count.load()){}

#pragma endregion

#pragma region Private member functions

/**
 * @brief enqueue operation, push a value into the audio queue.
 * 
 * use release - aquire model to ensure a sychornized-with relation between differnt therad.
 * 
 * @param value  Value to push.
 * @return true  If operation successed.
 * @return false If operation failed.
 */
template<audio_t T>
bool audioQueue<T>::enqueue(const T value)
{
    auto currTail = tail.load(std::memory_order_relaxed);
    auto nextTail = (currTail + 1) % queue.size(); 
    if (nextTail == head.load(std::memory_order_acquire)) 
        return false; // Queue is full
    queue[currTail] = value;
    tail .store    (nextTail, std::memory_order_release);
    count.fetch_add(       1, std::memory_order_relaxed);
    return true;
}

/**
 * @brief dequeue operation, pop a value from the queue.
 * 
 * use release - aquire model to ensure a sychornized-with relation between differnt therad.
 * 
 * @param value  The target variable for pop output.
 * @param mode   true    : do a addition that add poped element to value.
 *               false   : do a replacement that replace the origingal element in value.
 * @return true  If operation successed.
 * @return false If operation failed.
 */
template<audio_t T>
bool audioQueue<T>::dequeue(         T& value, 
                            const bool  mode)
{
    auto currHead = head.load(std::memory_order_relaxed);
    auto nextHead = (currHead + 1) % queue.size();
    if (currHead == tail.load(std::memory_order_acquire)) 
        return false; // Queue is empty
    if (!mode) value  = queue[currHead];//cover mode.
    else       value += queue[currHead];//addition mode.
    head .store    (nextHead, std::memory_order_release);
    count.fetch_sub(       1, std::memory_order_relaxed);
    return true;
}

template<audio_t T>
void audioQueue<T>::clear()
{
    head .store(0);
    tail .store(0);
    count.store(0);
}

/**
 * @brief resample a slice of audio using libsamplerate, from "inputSamplerate" to output sample rate.
 * 
 * @param data               Input vector
 * @param inputSampleRate    Input samplerate.
 * @param inputChannelNumber Input Channel numbers. For sample number calculation
 */
template<audio_t T>
void audioQueue<T>::resample(      std::vector<T>& data,
                             const std::uint32_t   inputSampleRate,
                             const std:: uint8_t   inputChannelNumber)
{
    const auto resampleRatio = static_cast<double>(outConfig.sampleRate) / inputSampleRate;
    const auto newSize       = static_cast<size_t>(data.size() * resampleRatio);
    const auto frames        = data.size() / inputChannelNumber;

    std::vector<float> floatData;

    if (std::is_same<T, short>::value)
    {
        floatData.resize(data.size());
        src_short_to_float_array(reinterpret_cast<const short*>(data.data()), floatData.data(), data.size());
    }
    else
        floatData = std::move(data);

    std::vector<float> temp(newSize);//new vector for resampling output.
    SRC_STATE* srcState = src_new(SRC_SINC_BEST_QUALITY, static_cast<int>(inputChannelNumber), nullptr);

    SRC_DATA srcData{};
    srcData.end_of_input  = true;
    srcData.data_in       = floatData.data();
    srcData.data_out      = temp.data();
    srcData.input_frames  = static_cast<long>(frames);
    srcData.output_frames = static_cast<long>(frames * resampleRatio);
    srcData.src_ratio     = resampleRatio;

    src_process(srcState, &srcData);
    src_delete (srcState);

    if (std::is_same<T, short>::value)
    {
        data.resize(temp.size());
        src_float_to_short_array(temp.data(), reinterpret_cast<short*>(data.data()), newSize);
    }
    else
        data = std::move(temp);
}

/**
 * @brief  Convert the number of audio channels to the expected output channel count.
 *
 * @param  data                  Input audio vector.
 * @param  originqlChannelNumber Input Channel numbers.
 */
template<audio_t T>
void audioQueue<T>::channelConvert(      std::vector<T>& data,
                                   const std:: uint8_t   originalChannelNumber)
{
    auto frames             = data.size() / originalChannelNumber;
    auto validChannelNumber = std::min(originalChannelNumber, outConfig.channelNumber);

    std::vector<T> convertedData(frames * outConfig.channelNumber);

    for (std::size_t i = 0; i < frames; i++)
        for (std::uint8_t j = 0; j < validChannelNumber; j++)
            convertedData[i * outConfig.channelNumber + j] = data[i * originalChannelNumber + j];

    data = std::move(convertedData);
}
#pragma endregion

#pragma region Public API
/**
 * @brief To push a numbers of samples into the buffer queue.
 * 
 * @param  data               Input audio vector.
 * @param  inputSampleRate    Input samplerate.       Resampling will be performed in the case where it is different from "outConfig.sampleRate".
 * @param  inputChannelNumber Input Channel numbers.  Channel convertion will be performed in cas where it is different from "outConfig.channelNumber".
 * @return true               If operation sucessed
 * @return false              If operation failed due to no enough space.
 */
template<audio_t T>
bool audioQueue<T>::push(      std::vector<T>&  data,
                         const std::uint32_t    inputSampleRate,
                         const std:: uint8_t    inputChannelNumber)
{
    if (inputSampleRate != outConfig.sampleRate)
        resample(data, inputSampleRate, inputChannelNumber);
    if (inputChannelNumber != outConfig.channelNumber)
        channelConvert(data, inputChannelNumber);

    for (const auto &i : data)
        if (!this->enqueue(i))
            return false;
    
    return true;
}
/**
 * @brief To pop a number of samples out of buffer queue
 * 
 * @param  data     Target output array.
 * @param  frames   Numbers of frames wanted.
 * @param  mode     true  : addition mode, adds audio data to array.
 *                  flase : cover mode, which will covers the original data in array.
 * @return true     If operation successed.
 * @return false    If operation failed / partially failed due to no enough samples in buffer.
 */
template<audio_t T>
bool audioQueue<T>::pop(                T*& data, 
                        const std::size_t   frames,
                        const        bool   mode)
{
    if (count.load(std::memory_order_acquire) < outConfig.minimumElement)
        return false;

    for (std::size_t i = 0; i < frames * outConfig.channelNumber; i++)
        if (!this->dequeue(data[i],mode))
            return false;

    return true;
}

/**
 * @brief To change max buffer size of audio queue.
 * 
 * Warning : this operation will delete all datas in the buffer queue ! 
 * Only call it when you do not need current audio data anymore.
 * 
 * @param newCapacity New queue capacity wanted.
 */
template<audio_t T>
void audioQueue<T>::setCapacity(const std::size_t newCapacity) 
{   
    if (newCapacity == queue.size()) 
        return;
    else
    {
        if (!this->empty()) 
            this->clear();

        queue.resize(newCapacity);
        outConfig.queueCapacity = newCapacity;
    }
}
#pragma endregion
#pragma endregion
#endif// AUDIO_QUEUE_H