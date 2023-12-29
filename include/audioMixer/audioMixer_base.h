#ifndef AUDIOMIXER_BASE_H
#define AUDIOMIXER_BASE_H

#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <print>
#include <string>

#include "audioMixer_queue.h"

/* Alias */
template <typename T>
using sPtr = std::shared_ptr<T>;

template <audio_t T>
using sPtrQueueList = std::shared_ptr<std::vector<audioQueue<T>>>;


/* Exceptions */
class modFatalErr : public std::runtime_error
{
public:
    modFatalErr(const char* msg) : std::runtime_error(msg) {}
    const char* what() const noexcept override { return std::runtime_error::what(); }
};
class modObjNotFound : public std::logic_error
{
public:
    modObjNotFound(const std::string& msg) : std::logic_error(msg) {}
    const char* what() const noexcept override { return std::logic_error::what(); }
};

/* tool functions */
template<typename T>
void parallelForEach(const std::vector<T>&                data, 
                     const std::function<void(const T&)>& func) 
{
    std::vector<std::jthread> threads;
    for (const auto& elem : data)
        threads.emplace_back([elem, &func](){ func(elem); });
}

/* input module base class */
class audioMixerModule_base
{
protected:
    sPtrQueueList<float> audio;
        outputParameter  outputConfig;
                   bool  active;
public:
    audioMixerModule_base() 
        :   active      (false){};
    audioMixerModule_base(const outputParameter& outputCfg) 
        :   active      (false), 
            outputConfig(outputCfg){ audio = std::make_shared<std::vector<audioQueue<float>>>(); }
    virtual ~audioMixerModule_base() {}    
    //any copy or move of a audioMixer module is NOT possible!
    audioMixerModule_base           (const audioMixerModule_base&  other) = delete;
    audioMixerModule_base           (      audioMixerModule_base&& other) = delete;
    audioMixerModule_base& operator=(const audioMixerModule_base&  other) = delete;
    audioMixerModule_base& operator=(      audioMixerModule_base&& other) = delete;
          
          virtual void start   () = 0;
          virtual void stop    () = 0;
    [[nodiscard]] auto getAudio() const { return audio ; }
    [[nodiscard]] bool isActive() const { return active; }
};
template<typename U>
concept inputMod_t = std::is_base_of<audioMixerModule_base, U>::value;
using module = audioMixerModule_base;
#endif//AUDIO_BASE_H