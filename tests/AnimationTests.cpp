#include "../Graphics/Animation/Animation.h"
#include "../Graphics/Assets/ModelAsset.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace dx = DirectX;
using namespace Animation;
namespace
{
int checks = 0;
void Check(bool condition, const char* description)
{
	++checks;
	if (!condition) throw std::runtime_error(description);
}
template<class F> void Reject(F fn, const char* description)
{
	bool rejected = false;
	try { fn(); } catch (const std::exception&) { rejected = true; }
	Check(rejected, description);
}
float Error(dx::FXMMATRIX a, dx::CXMMATRIX b)
{
	dx::XMFLOAT4X4 x, y; dx::XMStoreFloat4x4(&x, a); dx::XMStoreFloat4x4(&y, b);
	float result = 0;
	for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c)
	{
		if (!std::isfinite(y.m[r][c])) return 1e30f;
		result = (std::max)(result, std::abs(x.m[r][c] - y.m[r][c]));
	}
	return result;
}
SkeletonNode Node(const char* name, std::uint32_t parent, dx::FXMMATRIX local)
{
	SkeletonNode node; node.name = name; node.parent = parent;
	dx::XMStoreFloat4x4(&node.bindLocal, local);
	dx::XMVECTOR s, r, t; dx::XMMatrixDecompose(&s, &r, &t, local);
	dx::XMStoreFloat3(&node.scale, s); dx::XMStoreFloat4(&node.rotation, r); dx::XMStoreFloat3(&node.translation, t);
	return node;
}
void TestPose()
{
	Skeleton rig;
	rig.nodes = { Node("root", NoNode, dx::XMMatrixRotationY(0.4f) * dx::XMMatrixTranslation(3, 4, 5)),
		Node("bone", 0, dx::XMMatrixTranslation(0, 2, 0)), Node("mesh", 0, dx::XMMatrixTranslation(7, 0, 1)) };
	rig.boneNodes = { 1 };
	SkeletonPose bind; MakeBindPose(rig, bind);
	const auto mesh = dx::XMLoadFloat4x4(&bind.global[2]), bone = dx::XMLoadFloat4x4(&bind.global[1]);
	const auto offset = mesh * dx::XMMatrixInverse(nullptr, bone);
	Check(Error(SkinMatrix(offset, bone, mesh), dx::XMMatrixIdentity()) < 1e-5f, "Nonidentity root/mesh bind palette");
	AnimationClip clip; clip.id = "fixture"; clip.label = "fixture"; clip.duration = 2; clip.sourceRig = rig;
	NodeTrack track; track.nodeName = "bone";
	track.translations = { {0, {0,2,0}}, {2, {4,2,0}} };
	track.rotations = { {0, {0,0,0,1}}, {2, {0,0,1,0}} };
	track.scales = { {0, {1,1,1}}, {2, {3,1,1}} };
	clip.tracks.push_back(track);
	auto binding = BindClip(rig, clip);
	SkeletonPose pose; EvaluatePose(rig, clip, binding, 1, pose);
	const auto expected = dx::XMMatrixScaling(2, 1, 1) * dx::XMMatrixRotationZ(dx::XM_PIDIV2) * dx::XMMatrixTranslation(2, 2, 0);
	Check(Error(dx::XMLoadFloat4x4(&pose.local[1]), expected) < 1e-5f, "Halfway SRT interpolation");
	Check(Error(dx::XMLoadFloat4x4(&pose.global[1]), expected * dx::XMLoadFloat4x4(&bind.global[0])) < 1e-5f, "Parent accumulation order");
	Check(Error(dx::XMLoadFloat4x4(&pose.local[0]), dx::XMLoadFloat4x4(&rig.nodes[0].bindLocal)) == 0, "Missing channel keeps exact bind matrix");
	const auto skinned = SkinMatrix(offset, dx::XMLoadFloat4x4(&pose.global[1]), mesh);
	Check(Error(skinned * mesh, offset * expected * dx::XMLoadFloat4x4(&bind.global[0])) < 1e-4f, "Mesh/root transform applied once");
	const auto otherMesh = dx::XMMatrixScaling(2, 2, 2) * mesh;
	const auto otherOffset = otherMesh * dx::XMMatrixInverse(nullptr, bone);
	Check(Error(SkinMatrix(otherOffset, bone, otherMesh), dx::XMMatrixIdentity()) < 1e-5f, "Distinct inverse binds per mesh");
	clip.tracks[0].translations[0].interpolation = Interpolation::Step;
	clip.tracks[0].rotations = { {0, {0,0,0,1}}, {2, {0,0,0,-1}} };
	clip.tracks[0].scales.clear();
	EvaluatePose(rig, clip, binding, 1, pose);
	Check(Error(dx::XMLoadFloat4x4(&pose.local[1]), dx::XMMatrixTranslation(0,2,0)) < 1e-5f, "Step, quaternion sign equivalence, missing scale");
	clip.tracks[0].translations = { {1, {6,2,0}} };
	EvaluatePose(rig, clip, binding, 0, pose);
	Check(std::abs(pose.local[1]._41 - 6) < 1e-5f, "Single key before track start");
	EvaluatePose(rig, clip, binding, 20, pose);
	Check(std::abs(pose.local[1]._41 - 6) < 1e-5f, "Single key after track end");
	auto invalid = clip; invalid.tracks.push_back(invalid.tracks[0]);
	Reject([&] { ValidateClip(invalid); }, "Duplicate channel rejected");
	invalid = clip; invalid.tracks[0].translations.push_back(invalid.tracks[0].translations[0]);
	Reject([&] { ValidateClip(invalid); }, "Duplicate keys rejected");
	invalid = clip; invalid.tracks[0].translations[0].time = -1;
	Reject([&] { ValidateClip(invalid); }, "Negative key rejected");
	invalid = clip; invalid.tracks[0].rotations[0].value = {0,0,0,0};
	Reject([&] { ValidateClip(invalid); }, "Zero quaternion rejected");
	invalid = clip; invalid.tracks[0].translations[0].value.x = std::numeric_limits<float>::quiet_NaN();
	Reject([&] { ValidateClip(invalid); }, "Nonfinite key rejected");
	invalid = clip; invalid.sourceRig.nodes[1].parent = NoNode;
	Reject([&] { BindClip(rig, invalid); }, "Rig parent mismatch rejected");
	invalid = clip; invalid.sourceRig.nodes[1].bindLocal._42 += 10;
	Reject([&] { BindClip(rig, invalid); }, "Different bind rig rejected");
	invalid = clip; invalid.sourceRig.nodes[1].name = "different";
	Reject([&] { BindClip(rig, invalid); }, "Missing bone rejected");
	invalid = clip; invalid.sourceRig.nodes.push_back(invalid.sourceRig.nodes[1]);
	Reject([&] { BindClip(rig, invalid); }, "Ambiguous node rejected");
}
void TestClockAndWeights()
{
	PlaybackClock clock;
	clock.Play(); clock.Advance(0.5, 2, false, true);
	clock.Pause(); clock.Advance(1, 2, false, true);
	Check(clock.time == 0.5 && clock.state == PlaybackState::Paused, "Pause holds time");
	clock.Play(); clock.Advance(2, 2, false, true);
	Check(clock.time == 2 && clock.state == PlaybackState::Completed, "Nonloop holds final pose");
	clock.Play(); Check(clock.time == 0, "Play completed restarts");
	clock.Advance(5.25, 2, true, true); Check(clock.time == 1.25, "Loop handles multiple wraps");
	clock.Advance(1, 2, true, false); Check(clock.time == 1.25, "Gate freezes clock");
	clock.Select(); Check(clock.time == 0 && clock.state == PlaybackState::Playing, "Switch preserves Playing");
	clock.Seek(10, 2); Check(clock.time == 2 && clock.state == PlaybackState::Paused, "Seek clamps and pauses");
	clock.Restart(); Check(clock.time == 0 && clock.state == PlaybackState::Playing, "Restart plays");
	clock.Stop(); Check(clock.time == 0 && clock.state == PlaybackState::Stopped, "Stop resets");
	Check(CanAdvance(false, false) && CanAdvance(false, true), "Edit mode always permits playback");
	Check(CanAdvance(true, false) && !CanAdvance(true, true), "Only Play mode global pause gates playback");
	clock.Restart(); clock.Advance(0.1, 0, true, true); Check(clock.state == PlaybackState::Completed, "Zero-duration clip safe");
	std::size_t discarded = 0;
	auto weights = PackInfluences({{0,.1f},{1,.4f},{2,.2f},{3,.05f},{4,.25f}}, discarded);
	Check(discarded == 1 && weights.indices.x == 1 && weights.indices.y == 4, "Top four influences sorted");
	Check(std::abs(weights.weights.x + weights.weights.y + weights.weights.z + weights.weights.w - 1) < 1e-6, "Weights normalized");
	Check(PackInfluences({}, discarded).weights.x == 0, "Unweighted vertices remain rigid");
	Reject([&] { PackInfluences({{128,1}}, discarded); }, "Bone limit enforced");
	Reject([&] { PackInfluences({{0,-1}}, discarded); }, "Negative weight rejected");
}
void TestAssets()
{
	Reject([] { ImportAnimations("__missing_animation__.fbx"); }, "Invalid file rejected");
	const std::filesystem::path bodyPath = "Graphics/Models/Humanoid/Body.fbx";
	if (!std::filesystem::exists(bodyPath)) { std::cout << "SKIP local humanoid assets (not tracked)\n"; return; }
	auto body = ImportModel(bodyPath);
	Check(body->meshes.size() == 2 && body->HasSkin(), "Body mesh count/skin");
	SkeletonPose bind; MakeBindPose(body->skeleton, bind);
	for (const auto& mesh : body->meshes)
	{
		Check(mesh.skin.boneNodes.size() == 52, "Body bone count");
		for (const auto& vertex : mesh.vertices)
		{
			const auto& w = vertex.skin.weights;
			Check(std::abs(w.x+w.y+w.z+w.w-1) < 1e-5, "Body vertex weights normalized");
		}
	}
	std::vector<std::string> ids;
	for (const auto& test : { std::pair{"Walking",31.0/30.0}, std::pair{"Jump",65.0/30.0} })
	{
		auto clips = ImportAnimations(bodyPath.parent_path()/"Animations"/(std::string(test.first)+".fbx"));
		Check(clips.size() == 1, "Animation-only import");
		const auto& clip = *clips[0]; ids.push_back(clip.id);
		Check(std::abs(clip.duration-test.second) < 1e-5, "Imported clip duration");
		auto binding = BindClip(body->skeleton, clip);
		SkeletonPose pose; EvaluatePose(body->skeleton, clip, binding, clip.duration/2, pose);
		Check(pose.global.size() == body->skeleton.nodes.size(), "Real rig sampling");
		for (const auto& node : body->skeleton.nodes) for (auto m : node.meshes)
		{
			const auto& skin = body->meshes[m].skin;
			const auto index = &node-body->skeleton.nodes.data();
			for (std::size_t b=0;b<skin.boneNodes.size();++b)
				Check(Error(SkinMatrix(dx::XMLoadFloat4x4(&skin.inverseBinds[b]), dx::XMLoadFloat4x4(&bind.global[skin.boneNodes[b]]), dx::XMLoadFloat4x4(&bind.global[index])), dx::XMMatrixIdentity()) < .002f, "Body bind palette identity");
		}
		std::cout << test.first << ": " << clip.duration << " s, " << clip.tracks.size() << " tracks\n";
	}
	Check(ids[0] != ids[1], "Same internal clip name has distinct file identity");
}
}
int main()
{
	try { TestPose(); TestClockAndWeights(); TestAssets(); std::cout << "PASS " << checks << " animation checks\n"; return 0; }
	catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
