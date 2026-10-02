#include "AudioTestAccess.h"
#include "AudioFake/Audio.h"
#include "../Components/AudioClip.h"
#include "../Scene/GameObject.h"
#include "../imgui/imgui_internal.h"
#include <iostream>
#include <limits>
#include <type_traits>

using namespace std::chrono_literals;
namespace
{
int checks = 0;
void Check(bool value, const char* message)
{
	++checks;
	if (!value) throw std::runtime_error(message);
}
template<class F> void Wait(F&& predicate)
{
	const auto deadline = std::chrono::steady_clock::now() + 3s;
	while (!predicate())
	{
		if (std::chrono::steady_clock::now() >= deadline) throw std::runtime_error("Audio worker timed out");
		std::this_thread::sleep_for(1ms);
	}
}
size_t Count(const char* name, int id = -1)
{
	const auto events = AudioFake::Snapshot();
	return static_cast<size_t>(std::count_if(events.begin(), events.end(), [&](const auto& e)
		{ return e.name == name && (id < 0 || e.id == id); }));
}
int LastVoice()
{
	const auto events = AudioFake::Snapshot();
	for (auto it = events.rbegin(); it != events.rend(); ++it) if (it->name == "instance") return it->id;
	throw std::runtime_error("No voice created");
}
float LastValue(const char* name, int id)
{
	const auto events = AudioFake::Snapshot();
	for (auto it = events.rbegin(); it != events.rend(); ++it) if (it->name == name && it->id == id) return it->value;
	throw std::runtime_error("No matching audio event");
}
void Barrier(Audio& audio)
{
	const auto before = Count("master");
	audio.SetMasterVolume(0.75f);
	Wait([&] { return Count("master") > before; });
}

void TestConfiguration()
{
	static_assert(!std::is_copy_constructible_v<AudioClip> && !std::is_move_constructible_v<AudioClip>);
	AudioClip clip;
	Check(clip.IsType(ComponentType::AudioClip) && ComponentTypeToString(clip.GetType()) == "Audio Clip", "Component type registered");
	Check(clip.GetSound().empty() && clip.GetHandle() == InvalidAudioHandle && !clip.IsLooping(), "Empty defaults");
	Check(clip.GetVolume() == 1 && clip.GetPitch() == 0 && clip.GetPan() == 0, "Default controls");
	Check(!clip.Play() && !clip.PlayOneShot() && !clip.GetLastError().empty(), "Empty playback fails safely");
	Check(clip.Stop() && clip.Pause() && clip.Resume(), "Idle controls need no service");
	Check(clip.SetSound("효과.wav") && clip.SetVolume(0.3f) && clip.SetPitch(-0.5f) && clip.SetPan(1), "Configuration needs no service");
	Check(!clip.Play() && clip.GetHandle() == InvalidAudioHandle && !clip.GetLastError().empty(), "Missing service is reported");
	Check(!clip.SetSound(std::string("bad\0.wav", 8)) && clip.GetSound() == "효과.wav", "Invalid path preserves configuration");
	for (float invalid : { -2.0f, 2.0f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN() })
	{
		Check(!clip.SetVolume(invalid) && !clip.SetPitch(invalid) && !clip.SetPan(invalid), "Invalid/nonfinite controls rejected");
	}
	Check(clip.GetVolume() == 0.3f && clip.GetPitch() == -0.5f && clip.GetPan() == 1, "Rejected edits preserve controls");
	Check(clip.SetSound({}) && clip.GetLastError().empty(), "Clear recovers from errors");
}

void TestPlayback(Audio& audio)
{
	AudioClip clip("효과.wav");
	clip.SetLooping(true);
	Check(clip.SetVolume(0.4f) && clip.SetPitch(-0.2f) && clip.SetPan(0.3f) && clip.Play(), "Configured play queued");
	const auto handle = clip.GetHandle();
	Check(handle != InvalidAudioHandle, "Handle available immediately");
	Check(clip.SetVolume(0.7f) && clip.SetPitch(0.2f) && clip.SetPan(-0.3f) && clip.Pause() && clip.Resume(), "Immediate controls queued");
	Barrier(audio);
	const int voice = LastVoice();
	Check(LastValue("play", voice) == 1 && LastValue("volume", voice) == 0.7f && LastValue("pitch", voice) == 0.2f && LastValue("pan", voice) == -0.3f, "Loop and live controls reach backend");
	Check(Count("pause", voice) == 1 && Count("resume", voice) == 1, "Pause and resume reach owned voice");
	bool unicode = false;
	for (const auto& event : AudioFake::Snapshot()) if (event.path == L"효과.wav") unicode = true;
	Check(unicode, "UTF-8 path reaches worker intact");
	Check(clip.PlayOneShot() && clip.PlayOneShot() && clip.GetHandle() == handle, "Overlapping one-shots preserve ownership");
	clip.SetLooping(false);
	Barrier(audio);
	Check(Count("play", voice) == 1 && LastValue("play", voice) == 1, "Loop edit takes effect on next Play");
	Check(clip.Play() && clip.GetHandle() != handle, "Play replaces instance");
	Barrier(audio);
	Check(Count("instance-destroy", voice) == 1 && LastValue("play", LastVoice()) == 0, "Restart stops previous loop and applies loop setting");
	const int replacement = LastVoice();
	const auto replacementHandle = clip.GetHandle();
	Check(clip.SetSound(clip.GetSound()) && clip.GetHandle() == replacementHandle, "Same path keeps playback");
	Check(clip.SetSound("next.wav") && clip.GetHandle() == InvalidAudioHandle, "Changing path releases handle");
	Barrier(audio);
	Check(Count("instance-destroy", replacement) == 1 && Count("one-shot") == 2, "Path replacement stops owned voice and preserves one-shots");
	Check(clip.Play(), "Replacement sound plays");
	Barrier(audio);
	const int completed = LastVoice();
	AudioFake::finish = true;
	Wait([&] { return Count("instance-destroy", completed) == 1; });
	AudioFake::finish = false;
	Check(clip.Pause() && clip.Resume() && clip.SetVolume(0.2f) && clip.Stop(), "Completed handle controls are harmless");
	Check(clip.Play(), "Playback after completion");
	audio.StopAll();
	Check(clip.Play(), "Playback recovers after global StopAll");
	Check(clip.Stop() && clip.Stop() && clip.GetHandle() == InvalidAudioHandle, "Stop is idempotent");
	Barrier(audio);
	Check(Count("instance") == Count("instance-destroy"), "All owned playback released");
}

void TestSceneLifetime(Audio& audio)
{
	Scene scene;
	auto& owner = scene.CreateGameObject("audio owner");
	auto& first = owner.AddComponent<AudioClip>("music.wav");
	auto& second = owner.AddComponent<AudioClip>("ambience.wav");
	Check(owner.GetComponent<AudioClip>() == &first, "Script component lookup");
	first.SetLooping(true); second.SetLooping(true);
	Check(first.Play() && second.Play(), "Multiple clips play independently");
	Barrier(audio);
	const int secondVoice = LastVoice();
	const auto destroyed = Count("instance-destroy");
	Check(owner.RemoveComponent<AudioClip>() && owner.GetComponent<AudioClip>() == &second, "Pending removal excluded from lookup");
	scene.CleanupPendingComponentRemovals();
	Barrier(audio);
	Check(Count("instance-destroy") == destroyed + 1 && Count("instance-destroy", secondVoice) == 0, "Removing one component stops only its voice");
	auto& child = scene.CreateChildGameObject(owner, "child");
	Check(child.AddComponent<AudioClip>("child.wav").Play(), "Child playback");
	owner.Destroy();
	scene.CleanupDestroyedObjects();
	Barrier(audio);
	Check(Count("instance") == Count("instance-destroy"), "Subtree destruction releases every voice");
	Check(scene.CreateGameObject("reset").AddComponent<AudioClip>("reset.wav").Play(), "Reset fixture");
	scene.Clear();
	Barrier(audio);
	Check(Count("instance") == Count("instance-destroy"), "Scene reset releases voices");
}

void TestInspector(Audio& audio)
{
	Scene scene;
	auto& owner = scene.CreateGameObject("inspector");
	auto& clip = owner.AddComponent<AudioClip>();
	auto& other = owner.AddComponent<AudioClip>("other.wav");
	ImGui::CreateContext();
	auto& io = ImGui::GetIO();
	io.IniFilename = nullptr; io.DisplaySize = { 1600, 1200 }; io.DeltaTime = 1.0f / 60;
	unsigned char* pixels; int width, height;
	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
	std::string visible;
	auto frame = [&](ImGuiID activate = 0, bool input = false)
	{
		ImGui::NewFrame();
		ImGui::SetNextWindowSize({ 650, 1100 });
		ImGui::Begin("Audio controls");
		if (activate)
		{
			auto& context = *ImGui::GetCurrentContext();
			context.NavActivateId = context.NavActivateDownId = context.NavActivatePressedId = activate;
			context.NavActivateFlags = input ? ImGuiActivateFlags_PreferInput : 0;
		}
		ImGui::LogToBuffer();
		clip.OnInspector(); other.OnInspector();
		visible = ImGui::GetCurrentContext()->LogBuffer.c_str();
		ImGui::LogFinish(); ImGui::End(); ImGui::Render();
	};
	frame(); frame();
	const auto window = ImGui::FindWindowByName("Audio controls")->ID;
	const auto scope = ImHashStr(clip.GetId().ToString().c_str(), 0, window);
	auto id = [&](const char* label) { return ImHashStr(label, 0, scope); };
	auto click = [&](const char* label) { frame(id(label)); frame(); };
	auto enter = [&](const char* label, const char* value)
	{
		frame(id(label), true);
		Check(ImGui::GetCurrentContext()->ActiveId == id(label), "Inspector field activates");
		io.AddInputCharactersUTF8(value); frame();
		io.AddKeyEvent(ImGuiKey_Enter, true); frame();
		io.AddKeyEvent(ImGuiKey_Enter, false); frame();
	};
	Check(visible.find("Audio Clip") != std::string::npos && visible.find("WAV file") != std::string::npos, "Inspector shows clip controls");
	click("Play");
	Check(clip.GetHandle() == InvalidAudioHandle && clip.GetLastError().empty(), "Empty Play is disabled");
	enter("WAV file", "ui.wav");
	enter("Volume", "0.25"); enter("Pitch", "-0.5"); enter("Pan", "0.75");
	click("Loop"); click("Play");
	Barrier(audio);
	const int voice = LastVoice();
	Check(clip.GetSound() == "ui.wav" && LastValue("play", voice) == 1, "Inspector configures and plays clip");
	Check(LastValue("volume", voice) == 0.25f && LastValue("pitch", voice) == -0.5f && LastValue("pan", voice) == 0.75f, "Inspector sliders use public setters");
	Check(other.GetSound() == "other.wav" && other.GetHandle() == InvalidAudioHandle && other.GetVolume() == 1, "Inspector IDs isolate multiple clips");
	click("Pause"); click("Resume"); click("Play One Shot"); click("Stop");
	Barrier(audio);
	Check(Count("pause", voice) == 1 && Count("resume", voice) == 1 && Count("instance-destroy", voice) == 1, "Inspector transport controls operate on owned voice");
	Check(clip.GetHandle() == InvalidAudioHandle, "Inspector stop clears handle");
	click("Browse WAV...");
	Check(!ImGui::GetCurrentContext()->OpenPopupStack.empty(), "WAV browser opens");
	ImGui::ClosePopupToLevel(0, true); frame();
	Check(!clip.SetVolume(-1), "Error fixture");
	frame(); frame();
	Check(visible.find("Volume must be") != std::string::npos, "Inspector retains errors after browser use");
	click("Clear");
	Check(clip.GetSound().empty(), "Inspector clears clip");
	ImGui::DestroyContext();
}

void TestWorkerFailure(std::unique_ptr<Audio>& audio)
{
	AudioClip clip("failure.wav");
	clip.SetLooping(true);
	Check(clip.Play(), "Failure fixture plays");
	Barrier(*audio);
	AudioFake::failUpdate = true;
	Wait([&] { return AudioTestAccess::Failed(*audio); });
	Check(!clip.Pause() && !clip.Resume() && !clip.PlayOneShot() && !clip.Play() && !clip.Stop(), "Worker failures contained by component API");
	Check(!clip.SetVolume(0.3f) && clip.GetVolume() == 1 && !clip.GetLastError().empty(), "Failed setter preserves config and reports error");
	audio.reset(); // Also exercise destruction after service shutdown.
}
}

int main()
{
	try
	{
		TestConfiguration();
		AudioFake::Clear();
		auto audio = AudioTestAccess::Create();
		TestPlayback(*audio); TestSceneLifetime(*audio); TestInspector(*audio); TestWorkerFailure(audio);
		Check(AudioFake::violations == 0 && AudioFake::liveSounds == 0 && DirectX::instances.empty(), "Backend lifetime and thread ownership intact");
		std::cout << "AudioClip: " << checks << " checks passed\n";
		return 0;
	}
	catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
