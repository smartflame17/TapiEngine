#pragma once
#include <cstdint>
#include <string>
#include <variant>

// Zero is invalid. IDs are generated on the main thread and never reused.
using AudioHandle = std::uint64_t;
inline constexpr AudioHandle InvalidAudioHandle = 0;

namespace AudioCommands
{
struct PlayOneShot
{
    std::string sound;
    float volume = 1.0f;
    float pitch = 0.0f;
    float pan = 0.0f;
};

struct PlayInstance
{
    AudioHandle handle;
    std::string sound;
    bool loop = false;
    float volume = 1.0f;
    float pitch = 0.0f;
    float pan = 0.0f;
};

struct Stop { AudioHandle handle; };
struct Pause { AudioHandle handle; };
struct Resume { AudioHandle handle; };
struct SetVolume { AudioHandle handle; float value; };
struct SetPitch { AudioHandle handle; float value; };
struct SetPan { AudioHandle handle; float value; };
struct SetMasterVolume { float value; };
struct StopAll {};
struct Suspend {};
struct ResumeAll {};
struct ResetDevice {};
}

using AudioCommand = std::variant<AudioCommands::PlayOneShot, AudioCommands::PlayInstance,
    AudioCommands::Stop, AudioCommands::Pause, AudioCommands::Resume,
    AudioCommands::SetVolume, AudioCommands::SetPitch, AudioCommands::SetPan,
    AudioCommands::SetMasterVolume, AudioCommands::StopAll, AudioCommands::Suspend,
    AudioCommands::ResumeAll, AudioCommands::ResetDevice>;
