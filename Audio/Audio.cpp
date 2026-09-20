#include "Audio.h"
#include "../SmflmWin.h"
#include <Audio.h> // DirectXTK; deliberately private to this translation unit.
#include <chrono>
#include <cmath>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <utility>

#pragma comment(lib, "ole32.lib")

namespace
{
	using Clock = std::chrono::steady_clock;
	constexpr auto UpdateInterval = std::chrono::milliseconds(10);
	constexpr auto RetryInterval = std::chrono::seconds(1);

	void ReportError(const char* message) noexcept
	{
		// The editor's spdlog sink list is mutated on the main thread.
		OutputDebugStringA("Audio: ");
		OutputDebugStringA(message);
		OutputDebugStringA("\n");
		std::fprintf(stderr, "Audio: %s\n", message);
	}

	void ValidateValue(float value, float minimum, float maximum)
	{
		if (!std::isfinite(value) || value < minimum || value > maximum)
			throw std::invalid_argument("Audio volume must be in [0, 1]; pitch and pan in [-1, 1].");
	}

	void ValidateSound(const std::string& sound)
	{
		if (sound.empty() || sound.find('\0') != std::string::npos)
			throw std::invalid_argument("Audio requires a nonempty UTF-8 WAV filename.");
	}

	std::wstring WidePath(const std::string& path)
	{
		const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path.c_str(), -1, nullptr, 0);
		if (size == 0)
			throw std::invalid_argument("Invalid UTF-8 audio filename.");
		std::wstring wide(static_cast<size_t>(size), L'\0');
		if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path.c_str(), -1, wide.data(), size))
			throw std::runtime_error("Failed to convert audio filename.");
		wide.pop_back();
		return wide;
	}

	struct ComApartment
	{
		ComApartment()
		{
			if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)))
				throw std::runtime_error("Failed to initialize COM on the audio thread.");
		}
		~ComApartment() { CoUninitialize(); }
	};

	DirectX::AUDIO_ENGINE_FLAGS EngineFlags() noexcept
	{
		auto flags = DirectX::AudioEngine_Default;
#ifdef _DEBUG
		flags |= DirectX::AudioEngine_Debug;
#endif
		return flags;
	}

	// Constructed, used, and destroyed exclusively inside ThreadMain.
	class AudioState
	{
		struct Voice
		{
			std::unique_ptr<DirectX::SoundEffectInstance> instance;
			bool loop;
			bool paused = false;
		};
		// Reverse destruction order: instances, their source data, then engine.
		DirectX::AudioEngine engine{ EngineFlags() };
		std::unordered_map<std::string, std::unique_ptr<DirectX::SoundEffect>> sounds;
		std::unordered_map<AudioHandle, Voice> voices;
		bool suspended = false;
		float masterVolume = 1.0f;
		Clock::time_point retryAt{};

		DirectX::SoundEffect& Sound(const std::string& path)
		{
			const auto found = sounds.find(path);
			if (found != sounds.end()) return *found->second;
			auto sound = std::make_unique<DirectX::SoundEffect>(&engine, WidePath(path).c_str());
			return *sounds.emplace(path, std::move(sound)).first->second;
		}

		void Reset()
		{
			retryAt = Clock::now() + RetryInterval;
			if (!engine.Reset()) return;
			engine.SetMasterVolume(masterVolume);
			if (suspended) engine.Suspend();
			for (auto& [handle, voice] : voices)
			{
				if (!voice.loop) continue;
				try
				{
					voice.instance->Play(true);
					if (voice.paused) voice.instance->Pause();
				}
				catch (const std::exception& error) { ReportError(error.what()); }
			}
		}

	public:
		~AudioState() { engine.Suspend(); }

		void Process(const AudioCommands::PlayOneShot& command)
		{
			Sound(command.sound).Play(command.volume, command.pitch, command.pan);
		}
		void Process(const AudioCommands::PlayInstance& command)
		{
			auto voice = Sound(command.sound).CreateInstance();
			voice->SetVolume(command.volume);
			voice->SetPitch(command.pitch);
			voice->SetPan(command.pan);
			voice->Play(command.loop);
			voices.emplace(command.handle, Voice{ std::move(voice), command.loop });
		}
		void Process(const AudioCommands::Stop& command) { voices.erase(command.handle); }
		void Process(const AudioCommands::Pause& command)
		{
			if (auto it = voices.find(command.handle); it != voices.end())
			{
				it->second.instance->Pause();
				it->second.paused = true;
			}
		}
		void Process(const AudioCommands::Resume& command)
		{
			if (auto it = voices.find(command.handle); it != voices.end())
			{
				it->second.instance->Resume();
				it->second.paused = false;
			}
		}
		void Process(const AudioCommands::SetVolume& command)
		{
			if (auto it = voices.find(command.handle); it != voices.end()) it->second.instance->SetVolume(command.value);
		}
		void Process(const AudioCommands::SetPitch& command)
		{
			if (auto it = voices.find(command.handle); it != voices.end()) it->second.instance->SetPitch(command.value);
		}
		void Process(const AudioCommands::SetPan& command)
		{
			if (auto it = voices.find(command.handle); it != voices.end()) it->second.instance->SetPan(command.value);
		}
		void Process(const AudioCommands::SetMasterVolume& command)
		{
			engine.SetMasterVolume(command.value);
			masterVolume = command.value;
		}
		void Process(const AudioCommands::Suspend&)
		{
			engine.Suspend();
			suspended = true;
		}
		void Process(const AudioCommands::ResumeAll&)
		{
			engine.Resume();
			suspended = false;
		}
		void Process(const AudioCommands::StopAll&)
		{
			engine.Suspend();
			voices.clear();
			sounds.clear(); // SoundEffect destruction also stops its outstanding one-shots.
			engine.TrimVoicePool();
			if (!suspended) engine.Resume();
		}
		void Process(const AudioCommands::ResetDevice&) { Reset(); }

		void Update()
		{
			if (!engine.Update() && Clock::now() >= retryAt)
			{
				// Poll slowly in silent mode, including startup without an audio device.
				try { Reset(); }
				catch (const std::exception& error) { ReportError(error.what()); }
			}
			for (auto it = voices.begin(); it != voices.end();)
			{
				// Loops retain their handles in silent mode for device recovery.
				if (!it->second.loop && it->second.instance->GetState() == DirectX::STOPPED)
					it = voices.erase(it);
				else ++it;
			}
		}
	};
}

Audio* Audio::instance = nullptr;
AudioHandle Audio::nextHandle = 1;

Audio::Audio()
{
	if (instance != nullptr)
		throw std::logic_error("Only one App-owned Audio instance may be active.");
	std::promise<void> startup;
	auto ready = startup.get_future();
	worker = std::thread(&Audio::ThreadMain, this, std::move(startup));
	try { ready.get(); }
	catch (...)
	{
		worker.join();
		throw;
	}
	instance = this;
}

Audio::~Audio() noexcept
{
	{
		std::lock_guard<std::mutex> lock(commandMutex);
		shutdownRequested = true;
	}
	commandReady.notify_one();
	worker.join();
	instance = nullptr;
}

Audio& Audio::GetInstance()
{
	if (!instance)
		throw std::logic_error("Audio is only available during its owning App's lifetime.");
	return *instance;
}

void Audio::Submit(AudioCommand command)
{
	{
		std::lock_guard<std::mutex> lock(commandMutex);
		if (workerFailure) std::rethrow_exception(workerFailure);
		commands.push(std::move(command));
	}
	commandReady.notify_one();
}

void Audio::PlayOneShot(std::string sound, float volume, float pitch, float pan)
{
	ValidateSound(sound);
	ValidateValue(volume, 0.0f, 1.0f);
	ValidateValue(pitch, -1.0f, 1.0f);
	ValidateValue(pan, -1.0f, 1.0f);
	Submit(AudioCommands::PlayOneShot{ std::move(sound), volume, pitch, pan });
}

AudioHandle Audio::Play(std::string sound, bool loop, float volume, float pitch, float pan)
{
	ValidateSound(sound);
	ValidateValue(volume, 0.0f, 1.0f);
	ValidateValue(pitch, -1.0f, 1.0f);
	ValidateValue(pan, -1.0f, 1.0f);
	if (nextHandle == InvalidAudioHandle)
		throw std::overflow_error("Audio handle IDs exhausted.");
	const auto handle = nextHandle++;
	Submit(AudioCommands::PlayInstance{ handle, std::move(sound), loop, volume, pitch, pan });
	return handle;
}

void Audio::Stop(AudioHandle handle) { Submit(AudioCommands::Stop{ handle }); }
void Audio::Pause(AudioHandle handle) { Submit(AudioCommands::Pause{ handle }); }
void Audio::Resume(AudioHandle handle) { Submit(AudioCommands::Resume{ handle }); }
void Audio::SetVolume(AudioHandle handle, float volume)
{
	ValidateValue(volume, 0.0f, 1.0f);
	Submit(AudioCommands::SetVolume{ handle, volume });
}
void Audio::SetPitch(AudioHandle handle, float pitch)
{
	ValidateValue(pitch, -1.0f, 1.0f);
	Submit(AudioCommands::SetPitch{ handle, pitch });
}
void Audio::SetPan(AudioHandle handle, float pan)
{
	ValidateValue(pan, -1.0f, 1.0f);
	Submit(AudioCommands::SetPan{ handle, pan });
}
void Audio::SetMasterVolume(float volume)
{
	ValidateValue(volume, 0.0f, 1.0f);
	Submit(AudioCommands::SetMasterVolume{ volume });
}
void Audio::StopAll() { Submit(AudioCommands::StopAll{}); }
void Audio::Suspend() { Submit(AudioCommands::Suspend{}); }
void Audio::Resume() { Submit(AudioCommands::ResumeAll{}); }
void Audio::ResetDevice() { Submit(AudioCommands::ResetDevice{}); }

void Audio::ThreadMain(std::promise<void> startup) noexcept
{
	bool started = false;
	try
	{
		ComApartment com;
		AudioState state;
		startup.set_value();
		started = true;
		std::queue<AudioCommand> pending;
		auto nextUpdate = Clock::now();
		while (true)
		{
			{
				std::unique_lock<std::mutex> lock(commandMutex);
				commandReady.wait_until(lock, nextUpdate, [this] { return shutdownRequested || !commands.empty(); });
				if (shutdownRequested) break;
				pending.swap(commands);
			}
			while (!pending.empty())
			{
				try { std::visit([&state](const auto& command) { state.Process(command); }, pending.front()); }
				catch (const std::exception& error) { ReportError(error.what()); }
				pending.pop();
				// A large batch must not starve DirectXTK's voice maintenance.
				if (Clock::now() >= nextUpdate)
				{
					state.Update();
					nextUpdate = Clock::now() + UpdateInterval;
				}
			}
			if (Clock::now() >= nextUpdate)
			{
				state.Update();
				nextUpdate = Clock::now() + UpdateInterval;
			}
		}
	}
	catch (...)
	{
		const auto error = std::current_exception();
		{
			std::lock_guard<std::mutex> lock(commandMutex);
			workerFailure = error;
		}
		if (!started) startup.set_exception(error);
		else ReportError("Audio worker stopped after an unexpected failure; subsequent submissions will rethrow it.");
	}
}
