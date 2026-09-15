#pragma once
#include "../Animation/Animation.h"
#include "../PhongMaterial.h"
#include <DirectXCollision.h>
#include <filesystem>

struct MeshSkinBinding
{
	std::vector<std::uint32_t> boneNodes;
	std::vector<DirectX::XMFLOAT4X4> inverseBinds;
	std::vector<DirectX::BoundingBox> influenceBounds;
	std::vector<bool> hasInfluenceBounds;
	DirectX::BoundingBox rigidBounds = {};
	bool hasRigidBounds = false;
};
struct ModelVertex
{
	DirectX::XMFLOAT3 position, normal, tangent;
	DirectX::XMFLOAT2 uv;
	Animation::VertexInfluences skin;
};
struct MeshAsset
{
	std::string name;
	std::vector<ModelVertex> vertices;
	std::vector<std::uint32_t> indices;
	bool hasUV = false;
	PhongMaterial material;
	std::filesystem::path baseColorTexture, normalTexture;
	DirectX::BoundingBox bounds = {};
	MeshSkinBinding skin;
};
struct ModelAsset
{
	Animation::Skeleton skeleton;
	std::vector<MeshAsset> meshes;
	bool HasSkin() const noexcept { return !skeleton.boneNodes.empty(); }
};

// CPU-only import entry points. A custom-format reader can replace these without
// changing Model, Animator, or the renderer. No Assimp pointers escape this layer.
std::shared_ptr<const ModelAsset> ImportModel(const std::filesystem::path& path);
std::vector<std::shared_ptr<const Animation::AnimationClip>> ImportAnimations(const std::filesystem::path& path);
