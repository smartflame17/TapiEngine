#pragma once
#include "Component.h"
#include "../Graphics/Assets/ModelAsset.h"
#include "../imgui/imgui.h"
#include "../imgui/imfilebrowser.h"

class Model;

class Animator : public Component
{
public:
	static constexpr ComponentType StaticType = ComponentType::Animator;
	struct ClipEntry
	{
		std::shared_ptr<const Animation::AnimationClip> clip;
		Animation::ClipBinding binding;
		bool loop = true;
	};
	Animator();
	bool LoadAnimations(const std::filesystem::path& path, bool loop = true) noexcept;
	bool AddClip(std::shared_ptr<const Animation::AnimationClip> clip, bool loop = true) noexcept;
	void RemoveClip(std::size_t index) noexcept;
	void SelectClip(std::size_t index) noexcept;
	void Play() noexcept;
	void Pause() noexcept;
	void Restart() noexcept;
	void Stop() noexcept;
	void Seek(double seconds) noexcept;
	void SetSpeed(float speed) noexcept;
	void SetLooping(std::size_t index, bool loop) noexcept;
	const std::vector<ClipEntry>& GetClips() const noexcept { return clips; }
	std::size_t GetSelectedClip() const noexcept { return selected; }
	Animation::PlaybackState GetState() const noexcept { return clock.state; }
	double GetTime() const noexcept { return clock.time; }
	float GetSpeed() const noexcept { return clock.speed; }
	const std::string& GetStatus() const noexcept { return status; }
	// Called once per display frame by Scene, never by fixed-step OnUpdate.
	bool UpdateAnimation(float frameDelta, bool isPlayMode, bool isPaused) noexcept;
private:
	const char* GetInspectorTitle() const noexcept override { return "Animator"; }
	void DrawInspectorContents() noexcept override;
	Model* ResolveTarget(std::uint64_t& drawableId) noexcept;
	Model* ValidateTarget() noexcept;
	bool AddClips(const std::vector<std::shared_ptr<const Animation::AnimationClip>>& added, bool loop) noexcept;
	void ReportError(const std::string& error) noexcept;
	std::vector<ClipEntry> clips;
	std::size_t selected = 0;
	Animation::PlaybackClock clock;
	Animation::SkeletonPose evaluatedPose;
	std::shared_ptr<const ModelAsset> boundAsset;
	std::uint64_t boundDrawableId = 0;
	std::string status = "Load an animation file to begin.";
	std::string targetError;
	bool dirty = true;
	ImGui::FileBrowser fileBrowser;
};
