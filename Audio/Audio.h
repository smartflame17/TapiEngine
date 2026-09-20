#pragma once
#include "AudioCommand.h"
#include <condition_variable>
#include <exception>
#include <future>
#include <mutex>
#include <queue>
#include <thread>

class App;
// App-owned main-thread API. No DirectXTK objects cross the command queue.
// Calls are asynchronous; callers must not race App construction/destruction.
class Audio
{
public:

	~Audio() noexcept;
	Audio(const Audio&) = delete;
	Audio& operator=(const Audio&) = delete;
	Audio& operator=(Audio&&) = delete;
	Audio(Audio&&) = delete;

	static Audio& GetInstance();

	// Sound paths are UTF-8 WAV filenames, relative to the working directory.
	void PlayOneShot(std::string sound, float volume = 1.0f, float pitch = 0.0f, float pan = 0.0f);
	// A handle is returned before loading/playback; subsequent commands remain FIFO.
	AudioHandle Play(std::string sound, bool loop = false, float volume = 1.0f,
		float pitch = 0.0f, float pan = 0.0f);
	void Stop(AudioHandle handle);
	void Pause(AudioHandle handle);
	void Resume(AudioHandle handle);
	void SetVolume(AudioHandle handle, float volume);
	void SetPitch(AudioHandle handle, float pitch);
	void SetPan(AudioHandle handle, float pan);
	void SetMasterVolume(float volume);
	void StopAll(); // Stops instances and one-shots and releases cached sounds.
	void Suspend();
	void Resume();
	void ResetDevice();

private:
	friend class App;
	friend class AudioTestAccess;

	static Audio* instance;
	static AudioHandle nextHandle;
	Audio();
	void Submit(AudioCommand command);
	void ThreadMain(std::promise<void> startup) noexcept;

	std::queue<AudioCommand> commands;
	std::mutex commandMutex;
	std::condition_variable commandReady;
	bool shutdownRequested = false;
	std::exception_ptr workerFailure;
	std::thread worker;
};