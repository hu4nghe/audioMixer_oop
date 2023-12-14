#ifndef AUDIOMIXER_BASE_H
#define AUDIOMIXER_BASE_H

#include <cstdint>
#include <memory>sPtrQueueList
#include <print>

#include "audioMixer_queue.h"

template <audioType T>
using sPtrQueueList = std::shared_ptr<std::vector<audioQueue<T>>>;

class audioMixerModule_base
{
protected:
    //audioData
    sPtrQueueList<float> audio;
    //Config, status
    outputParameter outputConfig;
               bool active;
public:
    audioMixerModule_base() 
        :   active(false) {};
    audioMixerModule_base(const outputParameter& outputCfg) 
        :   active(false), 
            outputConfig(outputCfg)
    { audio = std::make_shared<std::vector<audioQueue<float>>>(); }

    //any copy or move of a audioMixer module is NOT possible!
    audioMixerModule_base           (const audioMixerModule_base&  other) = delete;
    audioMixerModule_base           (      audioMixerModule_base&& other) = delete;
    audioMixerModule_base& operator=(const audioMixerModule_base&  other) = delete;
    audioMixerModule_base& operator=(      audioMixerModule_base&& other) = delete;

    virtual ~audioMixerModule_base() {}
    virtual void start() = 0;
    virtual void stop () = 0;

    [[nodiscard]] auto getAudio() const -> sPtrQueueList<float> { return audio ; }
    [[nodiscard]] bool isActive() const                         { return active; }

    
};
/**
 * @brief
 *
 * @tparam T input module type : all derivated class of audioMixerModule_base, aka module.
 */
template<typename U>
concept inputMod_t = std::is_base_of<audioMixerModule_base, U>::value;
using module = audioMixerModule_base;
#endif//AUDIO_BASE_H