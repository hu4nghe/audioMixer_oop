#ifndef AUDIOMIXER_BASE_H
#define AUDIOMIXER_BASE_H

#include <cstdint>
#include <memory>
#include <print>

#include "audioMixer_queue.h"

class audioMixerModule_base
{
protected:
    //audioData
    std::shared_ptr<std::vector<audioQueue<float>>> audio;
    //Config, status
    outputParameter                outputConfig;
    bool                           active;
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

    auto getAudio() const -> std::shared_ptr<std::vector<audioQueue<float>>> { return audio; }
    [[nodiscard]] bool isActive() const { return active; }
};

using module = audioMixerModule_base;
#endif//AUDIO_BASE_H