#pragma once
// Test-only DirectXTK substitute. Compile the production Audio.cpp against this
// header to observe thread affinity, lifetimes, FIFO ordering and device loss.
#include <objbase.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace AudioFake
{
struct Event { std::string name; int id; float value; std::wstring path; };
inline std::mutex mutex;
inline std::vector<Event> events;
inline std::thread::id owner;
inline std::thread::id mainThread;
inline std::atomic<int> violations = 0;
inline std::atomic<bool> failStartup = false, failUpdate = false;
inline std::atomic<bool> device = true, canReset = true, finish = false;
inline int nextId = 1, liveSounds = 0;
inline bool suspended = false;

inline void Note(std::string name, int id = 0, float value = 0, std::wstring path = {})
{
    APTTYPE type;
    APTTYPEQUALIFIER qualifier;
    if (std::this_thread::get_id() != owner || owner == mainThread
        || FAILED(CoGetApartmentType(&type, &qualifier)) || type != APTTYPE_MTA)
        ++violations;
    std::lock_guard<std::mutex> lock(mutex);
    events.push_back({ std::move(name), id, value, std::move(path) });
}
inline std::vector<Event> Snapshot()
{
    std::lock_guard<std::mutex> lock(mutex);
    return events;
}
inline void Clear() // Only call when no Audio service exists.
{
    events.clear();
    violations = 0;
    failStartup = failUpdate = finish = false;
    device = canReset = true;
    nextId = 1;
    suspended = false;
    mainThread = std::this_thread::get_id();
}
}

namespace DirectX
{
using AUDIO_ENGINE_FLAGS = unsigned;
inline constexpr AUDIO_ENGINE_FLAGS AudioEngine_Default = 0, AudioEngine_Debug = 1;
enum SoundState { STOPPED, PLAYING, PAUSED };
class SoundEffectInstance;
inline std::vector<SoundEffectInstance*> instances;

class AudioEngine
{
public:
    explicit AudioEngine(AUDIO_ENGINE_FLAGS flags)
    {
        AudioFake::owner = std::this_thread::get_id();
        AudioFake::Note("engine", 0, static_cast<float>(flags));
        if (AudioFake::failStartup) throw std::runtime_error("Injected startup failure");
    }
    ~AudioEngine()
    {
        if (AudioFake::liveSounds || !instances.empty() || !AudioFake::suspended) ++AudioFake::violations;
        AudioFake::Note("engine-destroy");
    }
    bool Update();
    bool Reset();
    void Suspend() noexcept { AudioFake::suspended = true; AudioFake::Note("suspend"); }
    void Resume() { AudioFake::suspended = false; AudioFake::Note("resume-all"); }
    void SetMasterVolume(float value) { AudioFake::Note("master", 0, value); }
    void TrimVoicePool() { AudioFake::Note("trim"); }
};

class SoundEffect
{
public:
    int references = 0;
    int id;
    SoundEffect(AudioEngine*, const wchar_t* path) : id(AudioFake::nextId++)
    {
        AudioFake::Note("load", id, 0, path);
        if (std::wstring(path) == L"missing.wav") throw std::runtime_error("Injected missing WAV");
        if (std::wstring(path) == L"slow.wav") std::this_thread::sleep_for(std::chrono::milliseconds(200));
        ++AudioFake::liveSounds;
    }
    ~SoundEffect()
    {
        if (references) ++AudioFake::violations;
        --AudioFake::liveSounds;
        AudioFake::Note("sound-destroy", id);
    }
    void Play(float volume, float pitch, float pan)
    {
        AudioFake::Note("one-shot", id, volume);
        AudioFake::Note("one-shot-pitch", id, pitch);
        AudioFake::Note("one-shot-pan", id, pan);
    }
    std::unique_ptr<SoundEffectInstance> CreateInstance();
};

class SoundEffectInstance
{
    SoundEffect& source;
public:
    int id = AudioFake::nextId++;
    SoundState state = STOPPED;
    bool loop = false;
    explicit SoundEffectInstance(SoundEffect& sound) : source(sound)
    {
        ++source.references;
        instances.push_back(this);
        AudioFake::Note("instance", id);
    }
    ~SoundEffectInstance()
    {
        --source.references;
        instances.erase(std::find(instances.begin(), instances.end(), this));
        AudioFake::Note("instance-destroy", id);
    }
    void Play(bool looping)
    {
        loop = looping;
        state = AudioFake::device ? PLAYING : STOPPED;
        AudioFake::Note("play", id, loop ? 1.0f : 0.0f);
    }
    void Pause() noexcept { if (state == PLAYING) state = PAUSED; AudioFake::Note("pause", id); }
    void Resume() { if (state == PAUSED) state = PLAYING; AudioFake::Note("resume", id); }
    void SetVolume(float value) { AudioFake::Note("volume", id, value); }
    void SetPitch(float value) { AudioFake::Note("pitch", id, value); }
    void SetPan(float value) { AudioFake::Note("pan", id, value); }
    SoundState GetState() noexcept { AudioFake::Note("state", id); return state; }
};

inline std::unique_ptr<SoundEffectInstance> SoundEffect::CreateInstance()
{
    return std::make_unique<SoundEffectInstance>(*this);
}
inline bool AudioEngine::Update()
{
    AudioFake::Note("update");
    if (AudioFake::failUpdate) throw std::runtime_error("Injected update failure");
    if (AudioFake::finish && !AudioFake::suspended)
        for (auto* voice : instances) if (!voice->loop && voice->state == PLAYING) voice->state = STOPPED;
    return AudioFake::device;
}
inline bool AudioEngine::Reset()
{
    AudioFake::device = AudioFake::canReset.load();
    AudioFake::Note(AudioFake::device ? "reset" : "reset-failed");
    for (auto* voice : instances) voice->state = STOPPED;
    AudioFake::suspended = false;
    return AudioFake::device;
}
}
