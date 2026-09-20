#include "AudioTestAccess.h"
#include "AudioFake/Audio.h"
#include <functional>
#include <iostream>
#include <limits>

using namespace std::chrono_literals;
namespace
{
int checks = 0;
void Check(bool condition, const char* message)
{
    ++checks;
    if (!condition) throw std::runtime_error(message);
}
template<class F> void Throws(F&& operation)
{
    bool threw = false;
    try { operation(); } catch (const std::exception&) { threw = true; }
    Check(threw, "Expected synchronous exception");
}
template<class F> void Wait(F&& predicate)
{
    const auto deadline = std::chrono::steady_clock::now() + 3s;
    while (!predicate())
    {
        if (std::chrono::steady_clock::now() >= deadline) throw std::runtime_error("Timed out waiting for audio worker");
        std::this_thread::sleep_for(1ms);
    }
}
size_t Count(const char* name, int id = -1)
{
    const auto events = AudioFake::Snapshot();
    return static_cast<size_t>(std::count_if(events.begin(), events.end(), [&](const auto& e) {
        return e.name == name && (id < 0 || e.id == id);
    }));
}
void Barrier(Audio& audio)
{
    const auto before = Count("master");
    audio.SetMasterVolume(0.75f);
    Wait([&] { return Count("master") > before; });
}
void Finish(std::unique_ptr<Audio>& audio)
{
    audio.reset();
    Check(AudioFake::violations == 0, "Backend thread affinity / COM / destruction order");
    Check(AudioFake::liveSounds == 0 && DirectX::instances.empty(), "All backend resources released");
    Throws([] { Audio::GetInstance(); });
}

void CommandsAndLifetimes()
{
    AudioFake::Clear();
    Throws([] { Audio::GetInstance(); });
    auto audio = AudioTestAccess::Create();
    Check(&Audio::GetInstance() == audio.get(), "App-owned singleton access");
    Throws([] { AudioTestAccess::Create(); });
    Wait([] { return Count("update") >= 3; }); // Empty queue must still tick.
#ifdef _DEBUG
    Check(AudioFake::Snapshot().front().value == 1, "Debug engine flags");
#else
    Check(AudioFake::Snapshot().front().value == 0, "Release engine flags");
#endif
    const auto music = audio->Play("music.wav", true, 0.8f, 0.2f, -0.3f);
    const auto ambience = audio->Play("music.wav", true);
    Check(music != InvalidAudioHandle && ambience != music, "Unique main-thread handles");
    audio->SetVolume(music, 0.4f);
    audio->SetPitch(music, -0.5f);
    audio->SetPan(music, 0.6f);
    audio->Pause(music);
    audio->Resume(music);
    audio->PlayOneShot("music.wav", 0.3f, -0.2f, 0.1f);
    audio->PlayOneShot("music.wav");
    Barrier(*audio);
    Check(Count("load") == 1 && Count("one-shot") == 2, "Cache reused without merging one-shots");
    const auto events = AudioFake::Snapshot();
    int voice = 0;
    for (const auto& e : events) if (e.name == "instance") { voice = e.id; break; }
    std::vector<std::string> operations;
    std::vector<float> parameters;
    for (const auto& e : events)
        if (e.id == voice && e.name != "state") { operations.push_back(e.name); parameters.push_back(e.value); }
    Check(operations == std::vector<std::string>{ "instance", "volume", "pitch", "pan", "play", "volume", "pitch", "pan", "pause", "resume" }, "Handle commands dispatched in FIFO order");
    Check(parameters[1] == 0.8f && parameters[2] == 0.2f && parameters[3] == -0.3f
        && parameters[5] == 0.4f && parameters[6] == -0.5f && parameters[7] == 0.6f, "Playback parameters preserved");
    audio->Stop(music);
    audio->SetVolume(music, 0.1f);
    audio->Stop(music);
    audio->Stop(InvalidAudioHandle);
    Barrier(*audio);
    Check(Count("instance-destroy", voice) == 1 && Count("volume", voice) == 2, "Stopped and invalid handles are harmless");
    audio->Play("missing.wav", true);
    audio->PlayOneShot("\xff.wav");
    audio->PlayOneShot("효과.wav");
    Barrier(*audio);
    Check(Count("one-shot") == 3, "Bad files isolated; later commands execute");
    bool unicode = false;
    for (const auto& e : AudioFake::Snapshot()) if (e.path == L"효과.wav") unicode = true;
    Check(unicode, "UTF-8 paths converted on worker");
    Throws([&] { audio->Play(""); });
    Throws([&] { audio->SetVolume(music, -0.1f); });
    Throws([&] { audio->SetPitch(music, 1.1f); });
    Throws([&] { audio->SetPan(music, std::numeric_limits<float>::quiet_NaN()); });
    Throws([&] { audio->SetMasterVolume(std::numeric_limits<float>::infinity()); });
    audio->Suspend();
    audio->StopAll();
    Barrier(*audio);
    Check(Count("resume-all") == 0, "StopAll preserves global suspension");
    Check(Count("sound-destroy") == 2 && Count("instance-destroy") == 2, "StopAll releases instances and cached source data");
    audio->Resume();
    Barrier(*audio);
    Check(Count("resume-all") == 1, "Global resume dispatched");
    Finish(audio);
    AudioFake::Clear();
    audio = AudioTestAccess::Create();
    const auto replacement = audio->Play("new.wav", true);
    Check(replacement > ambience, "Handles never alias after service recreation");
    audio->Stop(music);
    Barrier(*audio);
    Check(Count("instance-destroy") == 0, "Previous service handles cannot stop new playback");
    Finish(audio);
}

void QueueAndCompletion()
{
    AudioFake::Clear();
    auto audio = AudioTestAccess::Create();
    const auto slow = audio->Play("slow.wav", true);
    Wait([] { return Count("load") == 1; });
    const auto start = std::chrono::steady_clock::now();
    audio->SetVolume(slow, 0.2f);
    audio->Stop(slow);
    Check(std::chrono::steady_clock::now() - start < 100ms, "Loading does not hold queue mutex or block submission");
    Barrier(*audio);
    const auto finishedBefore = Count("instance-destroy");
    audio->Play("short.wav");
    const auto paused = audio->Play("short.wav");
    audio->Pause(paused);
    Barrier(*audio);
    AudioFake::finish = true;
    Wait([&] { return Count("instance-destroy") == finishedBefore + 1; });
    audio->Resume(paused);
    Wait([&] { return Count("instance-destroy") == finishedBefore + 2; });
    for (int i = 0; i < 500; ++i)
    {
        const auto handle = audio->Play("stress.wav", true);
        audio->SetPitch(handle, 0.5f);
        audio->Stop(handle);
    }
    Barrier(*audio);
    Check(Count("instance-destroy") == finishedBefore + 502, "Burst of commands loses no handles or stops");
    Finish(audio);
    AudioFake::Clear();
    audio = AudioTestAccess::Create();
    for (int i = 0; i < 1000; ++i) audio->PlayOneShot("queued.wav");
    Finish(audio); // Shutdown with both local and shared queues possibly populated.
    AudioFake::Clear();
    audio = AudioTestAccess::Create();
    const auto shutdown = std::chrono::steady_clock::now();
    Finish(audio);
    Check(std::chrono::steady_clock::now() - shutdown < 500ms, "Idle shutdown wakes and joins worker");
}

void RecoveryAndFailures()
{
    AudioFake::Clear();
    AudioFake::device = AudioFake::canReset = false;
    auto audio = AudioTestAccess::Create();
    const auto loop = audio->Play("loop.wav", true);
    audio->Pause(loop);
    audio->Suspend();
    Barrier(*audio);
    Wait([] { return Count("reset-failed") >= 1; });
    std::this_thread::sleep_for(60ms);
    Check(Count("reset-failed") == 1 && Count("instance-destroy") == 0, "Silent retry is throttled; loop handle retained");
    AudioFake::canReset = true;
    Wait([] { return Count("reset") == 1 && Count("pause") >= 2; });
    Check(Count("play") == 2, "Loop restarts automatically when a device returns");
    audio->SetVolume(loop, 0.3f);
    Barrier(*audio);
    Check(Count("volume") == 2, "Original loop handle survives reset");
    audio->ResetDevice();
    Wait([] { return Count("reset") == 2 && Count("pause") >= 3; });
    const auto events = AudioFake::Snapshot();
    auto reset = std::find_if(events.rbegin(), events.rend(), [](const auto& e) { return e.name == "reset"; }).base();
    Check(std::find_if(reset, events.end(), [](const auto& e) { return e.name == "suspend"; }) != events.end(), "Reset preserves global suspension");
    audio->Stop(loop);
    Barrier(*audio);
    Finish(audio);
    AudioFake::Clear();
    AudioFake::failStartup = true;
    Throws([] { AudioTestAccess::Create(); });
    Throws([] { Audio::GetInstance(); });
    AudioFake::failStartup = false;
    audio = AudioTestAccess::Create();
    AudioFake::failUpdate = true;
    Wait([&] { return AudioTestAccess::Failed(*audio); });
    Throws([&] { audio->PlayOneShot("rejected.wav"); });
    Finish(audio);
}
}

int main()
{
    try
    {
        CommandsAndLifetimes();
        QueueAndCompletion();
        RecoveryAndFailures();
        std::cout << "Audio tests passed: " << checks << " checks\n";
        return 0;
    }
    catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
