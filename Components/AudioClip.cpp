#include "AudioClip.h"
#include "../Audio/Audio.h"
#include "../imgui/imgui.h"
#include "../imgui/imfilebrowser.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>
#include <vector>

namespace
{
	template<class F>
	bool TryAudioAction(std::string& error, F&& action) noexcept
	{
		try
		{
			action();
			error.clear();
			return true;
		}
		catch (const std::exception& e) { error = e.what(); }
		catch (...) { error = "Audio request failed."; }
		return false;
	}

	void ValidateSound(const std::string& sound)
	{
		if (sound.find('\0') != std::string::npos)
			throw std::invalid_argument("A WAV path cannot contain a null character.");
	}

	void ValidateValue(float value, float minimum, float maximum)
	{
		if (!std::isfinite(value) || value < minimum || value > maximum)
			throw std::invalid_argument("Volume must be in [0, 1]; pitch and pan in [-1, 1].");
	}
}

AudioClip::AudioClip(std::string sound) : Component(StaticType), sound(std::move(sound))
{
	ValidateSound(this->sound);
}

AudioClip::~AudioClip() noexcept { Stop(); }

bool AudioClip::Play() noexcept
{
	return TryAudioAction(lastError, [&]
	{
		if (sound.empty()) throw std::invalid_argument("Select a WAV file before playing.");
		auto& audio = Audio::GetInstance();
		if (handle != InvalidAudioHandle)
		{
			audio.Stop(handle);
			handle = InvalidAudioHandle;
		}
		handle = audio.Play(sound, looping, volume, pitch, pan);
	});
}

bool AudioClip::PlayOneShot() noexcept
{
	return TryAudioAction(lastError, [&]
	{
		if (sound.empty()) throw std::invalid_argument("Select a WAV file before playing.");
		Audio::GetInstance().PlayOneShot(sound, volume, pitch, pan);
	});
}

bool AudioClip::Pause() noexcept
{
	return TryAudioAction(lastError, [&]
	{
		if (handle != InvalidAudioHandle) Audio::GetInstance().Pause(handle);
	});
}

bool AudioClip::Resume() noexcept
{
	return TryAudioAction(lastError, [&]
	{
		if (handle != InvalidAudioHandle) Audio::GetInstance().Resume(handle);
	});
}

bool AudioClip::Stop() noexcept
{
	return TryAudioAction(lastError, [&]
	{
		if (handle != InvalidAudioHandle) Audio::GetInstance().Stop(handle);
		handle = InvalidAudioHandle;
	});
}

bool AudioClip::SetSound(std::string value) noexcept
{
	return TryAudioAction(lastError, [&]
	{
		ValidateSound(value);
		if (sound == value) return;
		if (handle != InvalidAudioHandle) Audio::GetInstance().Stop(handle);
		handle = InvalidAudioHandle;
		sound = std::move(value);
	});
}

bool AudioClip::SetVolume(float value) noexcept
{
	return TryAudioAction(lastError, [&]
	{
		ValidateValue(value, 0.0f, 1.0f);
		if (handle != InvalidAudioHandle) Audio::GetInstance().SetVolume(handle, value);
		volume = value;
	});
}

bool AudioClip::SetPitch(float value) noexcept
{
	return TryAudioAction(lastError, [&]
	{
		ValidateValue(value, -1.0f, 1.0f);
		if (handle != InvalidAudioHandle) Audio::GetInstance().SetPitch(handle, value);
		pitch = value;
	});
}

bool AudioClip::SetPan(float value) noexcept
{
	return TryAudioAction(lastError, [&]
	{
		ValidateValue(value, -1.0f, 1.0f);
		if (handle != InvalidAudioHandle) Audio::GetInstance().SetPan(handle, value);
		pan = value;
	});
}

void AudioClip::DrawInspectorContents() noexcept
{
	std::vector<char> path(sound.begin(), sound.end());
	path.resize((std::max)(path.size() + 256, size_t{ 1024 }), '\0');
	if (ImGui::InputText("WAV file", path.data(), path.size())) SetSound(path.data());
	if (ImGui::Button("Browse WAV..."))
	{
		TryAudioAction(lastError, [&]
		{
			if (!fileBrowser)
			{
				fileBrowser = std::make_unique<ImGui::FileBrowser>(ImGuiFileBrowserFlags_EditPathString);
				fileBrowser->SetTitle("Select WAV##" + GetId().ToString());
				fileBrowser->SetTypeFilters({ ".wav" });
			}
			fileBrowser->Open();
		});
	}
	ImGui::SameLine();
	if (ImGui::Button("Clear")) SetSound({});
	bool loop = looping;
	if (ImGui::Checkbox("Loop", &loop)) SetLooping(loop);
	ImGui::TextDisabled("Loop changes apply on next Play.");
	float editVolume = volume, editPitch = pitch, editPan = pan;
	if (ImGui::SliderFloat("Volume", &editVolume, 0, 1, "%.2f", ImGuiSliderFlags_AlwaysClamp)) SetVolume(editVolume);
	if (ImGui::SliderFloat("Pitch", &editPitch, -1, 1, "%.2f", ImGuiSliderFlags_AlwaysClamp)) SetPitch(editPitch);
	if (ImGui::SliderFloat("Pan", &editPan, -1, 1, "%.2f", ImGuiSliderFlags_AlwaysClamp)) SetPan(editPan);
	ImGui::BeginDisabled(sound.empty());
	if (ImGui::Button("Play")) Play();
	ImGui::SameLine();
	if (ImGui::Button("Play One Shot")) PlayOneShot();
	ImGui::EndDisabled();
	ImGui::BeginDisabled(handle == InvalidAudioHandle);
	if (ImGui::Button("Pause")) Pause();
	ImGui::SameLine();
	if (ImGui::Button("Resume")) Resume();
	ImGui::SameLine();
	if (ImGui::Button("Stop")) Stop();
	ImGui::EndDisabled();
	ImGui::TextWrapped("Play restarts this clip. One-shots can overlap and finish independently. Audio is audible in Play mode and follows global Pause/Stop.");
	if (fileBrowser)
	{
		try
		{
			fileBrowser->Display();
			if (fileBrowser->HasSelected())
			{
				const auto utf8 = fileBrowser->GetSelected().u8string();
				// u8string is string in C++17 and u8string in C++20.
				std::string selected(utf8.begin(), utf8.end());
				fileBrowser->ClearSelected();
				SetSound(std::move(selected));
			}
		}
		catch (const std::exception& e) { lastError = e.what(); }
	}
	if (!lastError.empty()) ImGui::TextWrapped("%s", lastError.c_str());
}
