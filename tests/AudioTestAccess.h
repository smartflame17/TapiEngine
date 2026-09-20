#pragma once
#include "../Audio/Audio.h"
#include <memory>

class AudioTestAccess
{
public:
    static std::unique_ptr<Audio> Create() { return std::unique_ptr<Audio>(new Audio); }
    static bool Failed(Audio& audio)
    {
        std::lock_guard<std::mutex> lock(audio.commandMutex);
        return audio.workerFailure != nullptr;
    }
};
