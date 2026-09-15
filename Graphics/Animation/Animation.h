#pragma once
#include <DirectXMath.h>
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Animation
{
constexpr std::uint32_t MaxBones = 128;
constexpr std::uint32_t NoNode = ~0u;
inline DirectX::XMFLOAT4X4 Identity()
{
	DirectX::XMFLOAT4X4 m;
	DirectX::XMStoreFloat4x4(&m, DirectX::XMMatrixIdentity());
	return m;
}

struct SkeletonNode
{
	std::string name;
	std::uint32_t parent = NoNode;
	DirectX::XMFLOAT4X4 bindLocal = Identity();
	DirectX::XMFLOAT3 translation = { 0, 0, 0 };
	DirectX::XMFLOAT4 rotation = { 0, 0, 0, 1 };
	DirectX::XMFLOAT3 scale = { 1, 1, 1 };
	std::vector<std::uint32_t> meshes;
};
struct Skeleton
{
	// Parent-before-child order, including non-bone hierarchy nodes.
	std::vector<SkeletonNode> nodes;
	std::vector<std::uint32_t> boneNodes;
};
struct SkeletonPose
{
	std::vector<DirectX::XMFLOAT4X4> local;
	std::vector<DirectX::XMFLOAT4X4> global;
};
enum class Interpolation { Step, Linear };
template<class T> struct Key
{
	double time = 0;
	T value;
	Interpolation interpolation = Interpolation::Linear;
};
struct NodeTrack
{
	std::string nodeName;
	std::vector<Key<DirectX::XMFLOAT3>> translations;
	std::vector<Key<DirectX::XMFLOAT4>> rotations;
	std::vector<Key<DirectX::XMFLOAT3>> scales;
};
struct AnimationClip
{
	std::string id;
	std::string label;
	double duration = 0;
	Skeleton sourceRig;
	std::vector<NodeTrack> tracks;
};
struct ClipBinding
{
	std::vector<std::int32_t> trackForNode;
};
struct VertexInfluences
{
	DirectX::XMUINT4 indices = { 0, 0, 0, 0 };
	DirectX::XMFLOAT4 weights = { 0, 0, 0, 0 };
};
// Transposed for HLSL column-major packing. Shared by the renderer and GPU tests.
struct SkinningConstants
{
	std::array<DirectX::XMFLOAT4X4, MaxBones> bones;
	std::array<DirectX::XMFLOAT4X4, MaxBones> normals;
};
static_assert(sizeof(SkinningConstants) == MaxBones * 128);
void PrepareSkinningConstants(const std::vector<DirectX::XMFLOAT4X4>& palette, SkinningConstants& output);

void ValidateSkeleton(const Skeleton& skeleton);
void ValidateClip(const AnimationClip& clip);
ClipBinding BindClip(const Skeleton& target, const AnimationClip& clip);
void AccumulatePose(const Skeleton& skeleton, SkeletonPose& pose);
void MakeBindPose(const Skeleton& skeleton, SkeletonPose& pose);
void EvaluatePose(const Skeleton& skeleton, const AnimationClip& clip,
	const ClipBinding& binding, double seconds, SkeletonPose& pose);
DirectX::XMMATRIX SkinMatrix(DirectX::FXMMATRIX inverseBind,
	DirectX::CXMMATRIX boneGlobal, DirectX::CXMMATRIX meshGlobal);
DirectX::XMMATRIX NormalMatrix(DirectX::FXMMATRIX matrix);
VertexInfluences PackInfluences(std::vector<std::pair<std::uint32_t, float>> influences,
	std::size_t& discarded);

enum class PlaybackState { Stopped, Playing, Paused, Completed };
class PlaybackClock
{
public:
	PlaybackState state = PlaybackState::Stopped;
	double time = 0;
	float speed = 1;
	void Play() noexcept;
	void Pause() noexcept;
	void Restart() noexcept;
	void Stop() noexcept;
	void Seek(double seconds, double duration) noexcept;
	void Select() noexcept;
	bool Advance(double delta, double duration, bool loop, bool allowed) noexcept;
};
bool CanAdvance(bool playMode, bool paused) noexcept;
}
