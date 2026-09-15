#include "Animator.h"
#include "DrawableComponent.h"
#include "../Graphics/Drawable/Model.h"
#include "../Scene/GameObject.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

Animator::Animator() : Component(StaticType)
{
	fileBrowser.SetTitle("Load Animation##" + std::to_string(GetId()));
	fileBrowser.SetTypeFilters({ ".fbx", ".gltf", ".glb", ".dae" });
}

void Animator::ReportError(const std::string& error) noexcept
{
	if (status != error) spdlog::warn("Animator: {}", error);
	status = error;
}

Model* Animator::ResolveTarget(std::uint64_t& drawableId) noexcept
{
	auto* owner = TryGetGameObject();
	if (!owner || owner->IsPendingKill()) { targetError = "Animator needs a live GameObject."; return nullptr; }
	Model* target = nullptr;
	int animators = 0, models = 0;
	for (const auto& component : owner->GetComponents())
	{
		if (component->IsPendingInspectorRemoval()) continue;
		if (component->IsType(StaticType)) ++animators;
		if (component->IsType(ComponentType::Drawable))
		{
			auto* drawable = static_cast<DrawableComponent*>(component.get());
			if (auto* model = dynamic_cast<Model*>(drawable->GetDrawable()); model && model->HasSkin())
			{
				target = model; drawableId = drawable->GetId(); ++models;
			}
		}
	}
	if (animators != 1 || models != 1)
	{
		targetError = "Animator requires one Animator and exactly one skinned Model on this GameObject.";
		return nullptr;
	}
	targetError.clear();
	return target;
}

Model* Animator::ValidateTarget() noexcept
{
	std::uint64_t drawableId = 0;
	auto* target = ResolveTarget(drawableId);
	if (!target) { boundAsset.reset(); boundDrawableId = 0; dirty = true; return nullptr; }
	if (target->GetAsset() == boundAsset && drawableId == boundDrawableId) return target;
	try
	{
		std::vector<Animation::ClipBinding> bindings;
		for (const auto& entry : clips) bindings.push_back(Animation::BindClip(target->GetAsset()->skeleton, *entry.clip));
		for (std::size_t i = 0; i < clips.size(); ++i) clips[i].binding = std::move(bindings[i]);
		boundAsset = target->GetAsset(); boundDrawableId = drawableId; dirty = true;
		return target;
	}
	catch (const std::exception& e)
	{
		targetError = std::string("Target rig is incompatible: ") + e.what();
		ReportError(targetError); boundAsset.reset(); boundDrawableId = 0;
		return nullptr;
	}
}

bool Animator::AddClips(const std::vector<std::shared_ptr<const Animation::AnimationClip>>& added, bool loop) noexcept
{
	if (!ValidateTarget()) { ReportError(targetError); return false; }
	try
	{
		std::vector<ClipEntry> staged;
		for (const auto& clip : added)
		{
			if (!clip) throw std::runtime_error("Cannot add a null animation clip.");
			const auto same = [&](const ClipEntry& entry) { return entry.clip->id == clip->id; };
			if (std::any_of(clips.begin(), clips.end(), same) || std::any_of(staged.begin(), staged.end(), same)) continue;
			staged.push_back({ clip, Animation::BindClip(boundAsset->skeleton, *clip), loop });
		}
		const auto count = staged.size();
		clips.reserve(clips.size() + staged.size());
		for (auto& entry : staged) clips.push_back(std::move(entry));
		status = count ? "Loaded " + std::to_string(count) + " animation clip(s)." : "These clips are already loaded.";
		return true;
	}
	catch (const std::exception& e) { ReportError(e.what()); return false; }
}
bool Animator::LoadAnimations(const std::filesystem::path& path, bool loop) noexcept
{
	try { return AddClips(ImportAnimations(path), loop); }
	catch (const std::exception& e) { ReportError(e.what()); return false; }
}
bool Animator::AddClip(std::shared_ptr<const Animation::AnimationClip> clip, bool loop) noexcept { return AddClips({ std::move(clip) }, loop); }
void Animator::RemoveClip(std::size_t index) noexcept
{
	if (index >= clips.size()) return;
	const bool active = index == selected;
	clips.erase(clips.begin() + index);
	if (index < selected) --selected;
	if (active)
	{
		selected = clips.empty() ? 0 : (std::min)(selected, clips.size() - 1);
		Stop();
	}
}
void Animator::SelectClip(std::size_t index) noexcept
{
	if (index >= clips.size() || selected == index) return;
	selected = index; clock.Select(); dirty = true;
}
void Animator::Play() noexcept { if (!clips.empty()) { clock.Play(); dirty = true; } }
void Animator::Pause() noexcept { clock.Pause(); }
void Animator::Restart() noexcept { if (!clips.empty()) { clock.Restart(); dirty = true; } }
void Animator::Stop() noexcept { clock.Stop(); dirty = true; }
void Animator::Seek(double seconds) noexcept { if (!clips.empty()) { clock.Seek(seconds, clips[selected].clip->duration); dirty = true; } }
void Animator::SetSpeed(float speed) noexcept { if (std::isfinite(speed) && speed > 0) clock.speed = speed; }
void Animator::SetLooping(std::size_t index, bool loop) noexcept { if (index < clips.size()) clips[index].loop = loop; }

bool Animator::UpdateAnimation(float delta, bool playMode, bool paused) noexcept
{
	auto* target = ValidateTarget();
	if (!target) return false;
	try
	{
		if (clips.empty() || clock.state == Animation::PlaybackState::Stopped)
		{
			target->ResetPose(); dirty = false; return true;
		}
		const auto& entry = clips[selected];
		dirty |= clock.Advance(delta, entry.clip->duration, entry.loop, Animation::CanAdvance(playMode, paused));
		if (dirty)
		{
			Animation::EvaluatePose(boundAsset->skeleton, *entry.clip, entry.binding, clock.time, evaluatedPose);
			target->SetPose(evaluatedPose); dirty = false;
		}
		return true;
	}
	catch (const std::exception& e) { ReportError(e.what()); clock.Pause(); return false; }
}

void Animator::DrawInspectorContents() noexcept
{
	const bool valid = ValidateTarget() != nullptr;
	if (!valid) ImGui::TextWrapped("%s", targetError.c_str());
	ImGui::BeginDisabled(!valid);
	if (ImGui::Button("Load Animation...")) fileBrowser.Open();
	if (ImGui::BeginCombo("Clip", clips.empty() ? "<none>" : clips[selected].clip->label.c_str()))
	{
		for (std::size_t i = 0; i < clips.size(); ++i)
		{
			ImGui::PushID(static_cast<int>(i));
			if (ImGui::Selectable(clips[i].clip->label.c_str(), i == selected)) SelectClip(i);
			ImGui::PopID();
		}
		ImGui::EndCombo();
	}
	ImGui::BeginDisabled(clips.empty());
	if (ImGui::Button(clock.state == Animation::PlaybackState::Paused ? "Resume" : "Play")) Play();
	ImGui::SameLine(); if (ImGui::Button("Pause")) Pause();
	ImGui::SameLine(); if (ImGui::Button("Restart")) Restart();
	ImGui::SameLine(); if (ImGui::Button("Stop")) Stop();
	float speed = clock.speed;
	if (ImGui::SliderFloat("Speed", &speed, 0.1f, 4.0f, "%.2fx")) SetSpeed(speed);
	if (!clips.empty())
	{
		auto& entry = clips[selected];
		bool loop = entry.loop;
		if (ImGui::Checkbox("Loop", &loop)) SetLooping(selected, loop);
		float time = static_cast<float>(clock.time);
		if (ImGui::SliderFloat("Time", &time, 0, static_cast<float>(entry.clip->duration), "%.3f s")) Seek(time);
		ImGui::Text("%.3f / %.3f seconds", clock.time, entry.clip->duration);
		ImGui::TextWrapped("%s", entry.clip->id.c_str());
	}
	ImGui::EndDisabled();
	ImGui::EndDisabled();
	// Removing an incompatible clip must remain available to recover a replaced rig.
	ImGui::BeginDisabled(clips.empty());
	if (ImGui::Button("Remove Clip")) RemoveClip(selected);
	ImGui::EndDisabled();
	static constexpr const char* states[] = { "Stopped (bind pose)", "Playing", "Paused", "Completed" };
	ImGui::Text("State: %s", states[static_cast<int>(clock.state)]);
	ImGui::TextWrapped("%s", status.c_str());
	ImGui::TextWrapped("Playback works in Edit and Play modes. Global Pause freezes playback in Play mode.");
	// Display every frame while open, rather than only on the button-click frame.
	fileBrowser.Display();
	if (fileBrowser.HasSelected())
	{
		LoadAnimations(fileBrowser.GetSelected());
		fileBrowser.ClearSelected();
	}
}
