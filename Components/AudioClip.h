#pragma once
#include "Component.h"
#include "../Audio/AudioCommand.h"
#include <memory>

namespace ImGui { class FileBrowser; }

class AudioClip final : public Component
{
public:
	static constexpr ComponentType StaticType = ComponentType::AudioClip;
	explicit AudioClip(std::string sound = {});
	~AudioClip() noexcept override;
	AudioClip(const AudioClip&) = delete;
	AudioClip& operator=(const AudioClip&) = delete;
	AudioClip(AudioClip&&) = delete;
	AudioClip& operator=(AudioClip&&) = delete;

	// Main-thread API. Success means queued, not successfully loaded or audible.
	// Failures return false and leave a message in GetLastError().
	bool Play() noexcept; // Starts from the beginning, replacing the owned instance.
	bool PlayOneShot() noexcept; // Overlapping, unowned playback; ignores looping.
	bool Pause() noexcept;
	bool Resume() noexcept;
	bool Stop() noexcept; // Also called on destruction; does not stop one-shots.

	// UTF-8 WAV path. Changing/clearing it stops the owned instance.
	bool SetSound(std::string sound) noexcept;
	const std::string& GetSound() const noexcept { return sound; }
	void SetLooping(bool value) noexcept { looping = value; } // Applies on next Play.
	bool IsLooping() const noexcept { return looping; }
	bool SetVolume(float value) noexcept; // [0, 1]; setters also update the instance.
	bool SetPitch(float value) noexcept; // [-1, 1]
	bool SetPan(float value) noexcept; // [-1, 1]
	float GetVolume() const noexcept { return volume; }
	float GetPitch() const noexcept { return pitch; }
	float GetPan() const noexcept { return pan; }
	// The subsystem may have completed/failed/stopped this handle asynchronously.
	AudioHandle GetHandle() const noexcept { return handle; }
	const std::string& GetLastError() const noexcept { return lastError; }

private:
	const char* GetInspectorTitle() const noexcept override { return "Audio Clip"; }
	void DrawInspectorContents() noexcept override;
	std::string sound;
	std::string lastError;
	AudioHandle handle = InvalidAudioHandle;
	bool looping = false;
	float volume = 1.0f;
	float pitch = 0.0f;
	float pan = 0.0f;
	std::unique_ptr<ImGui::FileBrowser> fileBrowser;
};
