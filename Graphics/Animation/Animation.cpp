#include "Animation.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace Animation
{
namespace dx = DirectX;
namespace
{
using NodeMap = std::unordered_map<std::string, std::uint32_t>;
NodeMap MapNodes(const Skeleton& skeleton)
{
	NodeMap result;
	for (std::uint32_t i = 0; i < skeleton.nodes.size(); ++i)
		if (!result.emplace(skeleton.nodes[i].name, i).second)
			throw std::runtime_error("Ambiguous rig node name: " + skeleton.nodes[i].name);
	return result;
}
bool Finite(const dx::XMFLOAT3& v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
bool Finite(const dx::XMFLOAT4& v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z) && std::isfinite(v.w); }
template<class T> void ValidateKeys(const std::vector<Key<T>>& keys, const std::string& name)
{
	double previous = -1;
	for (const auto& key : keys)
	{
		if (!std::isfinite(key.time) || key.time < 0 || key.time <= previous || !Finite(key.value))
			throw std::runtime_error("Invalid or duplicate animation key: " + name);
		if (key.interpolation != Interpolation::Step && key.interpolation != Interpolation::Linear)
			throw std::runtime_error("Unsupported interpolation: " + name);
		previous = key.time;
	}
}
template<class T, class Fn> T Sample(const std::vector<Key<T>>& keys, double time, const T& fallback, Fn interpolate)
{
	if (keys.empty()) return fallback;
	if (time <= keys.front().time) return keys.front().value;
	if (time >= keys.back().time) return keys.back().value;
	const auto right = std::upper_bound(keys.begin(), keys.end(), time,
		[](double t, const Key<T>& key) { return t < key.time; });
	const auto& left = *(right - 1);
	if (left.interpolation == Interpolation::Step) return left.value;
	return interpolate(left.value, right->value, float((time - left.time) / (right->time - left.time)));
}
}

void ValidateSkeleton(const Skeleton& skeleton)
{
	if (skeleton.nodes.empty()) throw std::runtime_error("Missing skeleton hierarchy.");
	for (std::size_t i = 0; i < skeleton.nodes.size(); ++i)
	{
		const auto& node = skeleton.nodes[i];
		if (node.parent != NoNode && node.parent >= i)
			throw std::runtime_error("Skeleton must have parent-before-child ordering.");
		for (auto& row : node.bindLocal.m) for (float value : row)
			if (!std::isfinite(value)) throw std::runtime_error("Nonfinite node transform: " + node.name);
	}
	for (auto index : skeleton.boneNodes)
		if (index >= skeleton.nodes.size()) throw std::runtime_error("Invalid skeleton bone index.");
}
void ValidateClip(const AnimationClip& clip)
{
	ValidateSkeleton(clip.sourceRig);
	MapNodes(clip.sourceRig);
	if (!std::isfinite(clip.duration) || clip.duration < 0 || clip.tracks.empty())
		throw std::runtime_error("Animation has no valid duration/tracks: " + clip.label);
	std::unordered_set<std::string> names;
	for (const auto& track : clip.tracks)
	{
		if (!names.insert(track.nodeName).second) throw std::runtime_error("Duplicate animation channel: " + track.nodeName);
		ValidateKeys(track.translations, track.nodeName);
		ValidateKeys(track.rotations, track.nodeName);
		ValidateKeys(track.scales, track.nodeName);
		for (const auto& key : track.rotations)
			if (dx::XMVectorGetX(dx::XMVector4LengthSq(dx::XMLoadFloat4(&key.value))) < 1e-12f)
				throw std::runtime_error("Zero animation quaternion: " + track.nodeName);
		for (const auto& key : track.scales)
			if (key.value.x <= 0 || key.value.y <= 0 || key.value.z <= 0)
				throw std::runtime_error("Animation requires positive, nonsingular bone scales: " + track.nodeName);
		auto checkEnd = [&](const auto& keys) {
			if (!keys.empty() && keys.back().time > clip.duration + 1e-6)
				throw std::runtime_error("Animation key exceeds clip duration: " + track.nodeName);
		};
		checkEnd(track.translations); checkEnd(track.rotations); checkEnd(track.scales);
	}
}
ClipBinding BindClip(const Skeleton& target, const AnimationClip& clip)
{
	ValidateSkeleton(target);
	ValidateClip(clip);
	const auto targets = MapNodes(target);
	const auto sources = MapNodes(clip.sourceRig);
	ClipBinding binding;
	binding.trackForNode.resize(target.nodes.size(), -1);
	std::unordered_set<std::uint32_t> required(target.boneNodes.begin(), target.boneNodes.end());
	for (std::size_t i = 0; i < clip.tracks.size(); ++i)
	{
		const auto& name = clip.tracks[i].nodeName;
		const auto it = targets.find(name);
		if (it == targets.end() || !sources.count(name)) throw std::runtime_error("Animation node is missing: " + name);
		binding.trackForNode[it->second] = static_cast<std::int32_t>(i);
		required.insert(it->second);
	}
	// Compare each required node and its ancestors, excluding unrelated mesh nodes.
	for (auto index : std::vector<std::uint32_t>(required.begin(), required.end()))
		for (auto parent = target.nodes[index].parent; parent != NoNode; parent = target.nodes[parent].parent)
			required.insert(parent);
	for (auto index : required)
	{
		const auto& node = target.nodes[index];
		const auto it = sources.find(node.name);
		if (it == sources.end()) throw std::runtime_error("Clip is missing rig node: " + node.name);
		const auto& source = clip.sourceRig.nodes[it->second];
		const auto parentName = [&](const Skeleton& s, const SkeletonNode& n) {
			return n.parent == NoNode ? std::string{} : s.nodes[n.parent].name;
		};
		if (parentName(target, node) != parentName(clip.sourceRig, source))
			throw std::runtime_error("Rig parent mismatch: " + node.name);
		for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c)
		{
			const float a = node.bindLocal.m[r][c], b = source.bindLocal.m[r][c];
			if (std::abs(a - b) > 1e-3f + 1e-4f * (std::max)(std::abs(a), std::abs(b)))
				throw std::runtime_error("Rig bind transform mismatch: " + node.name);
		}
	}
	return binding;
}
void AccumulatePose(const Skeleton& skeleton, SkeletonPose& pose)
{
	if (pose.local.size() != skeleton.nodes.size()) throw std::runtime_error("Pose size differs from skeleton.");
	pose.global.resize(pose.local.size());
	for (std::size_t i = 0; i < pose.local.size(); ++i)
	{
		auto global = dx::XMLoadFloat4x4(&pose.local[i]);
		if (skeleton.nodes[i].parent != NoNode)
			global *= dx::XMLoadFloat4x4(&pose.global[skeleton.nodes[i].parent]);
		dx::XMStoreFloat4x4(&pose.global[i], global);
	}
}
void MakeBindPose(const Skeleton& skeleton, SkeletonPose& pose)
{
	pose.local.resize(skeleton.nodes.size());
	for (std::size_t i = 0; i < skeleton.nodes.size(); ++i) pose.local[i] = skeleton.nodes[i].bindLocal;
	AccumulatePose(skeleton, pose);
}
void EvaluatePose(const Skeleton& skeleton, const AnimationClip& clip, const ClipBinding& binding, double seconds, SkeletonPose& pose)
{
	if (!std::isfinite(seconds) || binding.trackForNode.size() != skeleton.nodes.size())
		throw std::runtime_error("Invalid animation sample or binding.");
	pose.local.resize(skeleton.nodes.size());
	const auto lerp = [](const dx::XMFLOAT3& a, const dx::XMFLOAT3& b, float t) {
		dx::XMFLOAT3 result; dx::XMStoreFloat3(&result, dx::XMVectorLerp(dx::XMLoadFloat3(&a), dx::XMLoadFloat3(&b), t)); return result;
	};
	const auto slerp = [](const dx::XMFLOAT4& a, const dx::XMFLOAT4& b, float t) {
		dx::XMFLOAT4 result;
		dx::XMStoreFloat4(&result, dx::XMQuaternionNormalize(dx::XMQuaternionSlerp(
			dx::XMQuaternionNormalize(dx::XMLoadFloat4(&a)), dx::XMQuaternionNormalize(dx::XMLoadFloat4(&b)), t)));
		return result;
	};
	for (std::size_t i = 0; i < skeleton.nodes.size(); ++i)
	{
		const auto& node = skeleton.nodes[i];
		const auto trackIndex = binding.trackForNode[i];
		if (trackIndex < 0) { pose.local[i] = node.bindLocal; continue; }
		const auto& track = clip.tracks.at(trackIndex);
		const auto t = Sample(track.translations, seconds, node.translation, lerp);
		const auto r = Sample(track.rotations, seconds, node.rotation, slerp);
		const auto s = Sample(track.scales, seconds, node.scale, lerp);
		dx::XMStoreFloat4x4(&pose.local[i], dx::XMMatrixScaling(s.x, s.y, s.z) *
			dx::XMMatrixRotationQuaternion(dx::XMQuaternionNormalize(dx::XMLoadFloat4(&r))) * dx::XMMatrixTranslation(t.x, t.y, t.z));
	}
	AccumulatePose(skeleton, pose);
}
dx::XMMATRIX SkinMatrix(dx::FXMMATRIX offset, dx::CXMMATRIX bone, dx::CXMMATRIX mesh)
{
	return offset * bone * dx::XMMatrixInverse(nullptr, mesh);
}
dx::XMMATRIX NormalMatrix(dx::FXMMATRIX matrix)
{
	// Translation is irrelevant for directions. Avoid NaNs for zero editor scale.
	auto linear = matrix;
	linear.r[3] = dx::XMVectorSet(0, 0, 0, 1);
	const float det = dx::XMVectorGetX(dx::XMMatrixDeterminant(linear));
	return std::isfinite(det) && std::abs(det) > 1e-20f
		? dx::XMMatrixTranspose(dx::XMMatrixInverse(nullptr, linear)) : dx::XMMatrixIdentity();
}
VertexInfluences PackInfluences(std::vector<std::pair<std::uint32_t, float>> influences, std::size_t& discarded)
{
	for (const auto& value : influences)
		if (value.first >= MaxBones || !std::isfinite(value.second) || value.second < 0)
			throw std::runtime_error("Invalid bone influence.");
	std::erase_if(influences, [](const auto& value) { return value.second == 0; });
	std::stable_sort(influences.begin(), influences.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
	if (influences.size() > 4) { discarded += influences.size() - 4; influences.resize(4); }
	VertexInfluences result;
	double total = 0;
	for (const auto& value : influences) total += value.second;
	std::uint32_t indices[4] = {};
	float weights[4] = {};
	for (std::size_t i = 0; i < influences.size(); ++i) { indices[i] = influences[i].first; weights[i] = float(influences[i].second / total); }
	result.indices = dx::XMUINT4(indices);
	result.weights = dx::XMFLOAT4(weights);
	return result;
}
void PrepareSkinningConstants(const std::vector<dx::XMFLOAT4X4>& palette, SkinningConstants& output)
{
	if (palette.size() > MaxBones) throw std::runtime_error("Skin palette exceeds 128 bones.");
	output.bones.fill(Identity()); output.normals.fill(Identity());
	for (std::size_t i = 0; i < palette.size(); ++i)
	{
		const auto matrix = dx::XMLoadFloat4x4(&palette[i]);
		dx::XMStoreFloat4x4(&output.bones[i], dx::XMMatrixTranspose(matrix));
		dx::XMStoreFloat4x4(&output.normals[i], dx::XMMatrixTranspose(NormalMatrix(matrix)));
	}
}
void PlaybackClock::Play() noexcept { if (state == PlaybackState::Completed) time = 0; state = PlaybackState::Playing; }
void PlaybackClock::Pause() noexcept { if (state == PlaybackState::Playing) state = PlaybackState::Paused; }
void PlaybackClock::Restart() noexcept { time = 0; state = PlaybackState::Playing; }
void PlaybackClock::Stop() noexcept { time = 0; state = PlaybackState::Stopped; }
void PlaybackClock::Seek(double seconds, double duration) noexcept
{
	if (!std::isfinite(seconds)) return;
	time = std::clamp(seconds, 0.0, (std::max)(duration, 0.0)); state = PlaybackState::Paused;
}
void PlaybackClock::Select() noexcept { time = 0; if (state == PlaybackState::Completed) state = PlaybackState::Paused; }
bool PlaybackClock::Advance(double delta, double duration, bool loop, bool allowed) noexcept
{
	if (!allowed || state != PlaybackState::Playing || !std::isfinite(delta) || delta <= 0 || !std::isfinite(speed) || speed <= 0) return false;
	time += delta * speed;
	if (duration <= 0) { time = 0; state = PlaybackState::Completed; }
	else if (time >= duration)
	{
		if (loop) time = std::fmod(time, duration);
		else { time = duration; state = PlaybackState::Completed; }
	}
	return true;
}
bool CanAdvance(bool playMode, bool paused) noexcept
{
	return !playMode || !paused;
}
}
