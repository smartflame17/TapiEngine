#include "ModelAsset.h"
#include <assimp/Importer.hpp>
#include <assimp/config.h>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <unordered_map>

namespace
{
	PhongMaterial LoadMaterialConstants(const aiMaterial* pMaterial)
	{
		PhongMaterial material;
		if (pMaterial == nullptr)
		{
			return material;
		}

		aiColor4D baseColor;
		if (pMaterial->Get(AI_MATKEY_BASE_COLOR, baseColor) == aiReturn_SUCCESS ||
			pMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, baseColor) == aiReturn_SUCCESS)
		{
			material.color = { baseColor.r, baseColor.g, baseColor.b };
		}

		aiColor4D specularColor;
		if (pMaterial->Get(AI_MATKEY_COLOR_SPECULAR, specularColor) == aiReturn_SUCCESS)
		{
			material.specularColor = { specularColor.r, specularColor.g, specularColor.b };
		}

		float shininess = 0.0f;
		if (pMaterial->Get(AI_MATKEY_SHININESS, shininess) == aiReturn_SUCCESS && shininess > 0.0f)
		{
			material.specularPower = shininess;
		}

		float specularStrength = 0.0f;
		if (pMaterial->Get(AI_MATKEY_SHININESS_STRENGTH, specularStrength) == aiReturn_SUCCESS)
		{
			material.specularIntensity = specularStrength;
		}

		return material;
	}

	std::optional<std::filesystem::path> ResolveTexturePath(const aiMaterial* pMaterial, aiTextureType textureType, const std::filesystem::path& modelDirectory)
	{
		if (pMaterial == nullptr || pMaterial->GetTextureCount(textureType) == 0)
		{
			return std::nullopt;
		}

		aiString texturePath;
		if (pMaterial->GetTexture(textureType, 0u, &texturePath) != aiReturn_SUCCESS)
		{
			return std::nullopt;
		}

		const std::string_view relativePath{ texturePath.C_Str() };
		if (relativePath.empty() || relativePath.front() == '*')
		{
			return std::nullopt;
		}

		auto resolvedPath = modelDirectory / std::filesystem::path(relativePath);
		if (!std::filesystem::exists(resolvedPath))
		{
			return std::nullopt;
		}

		return resolvedPath.lexically_normal();
	}

	std::filesystem::path ResolveBaseColorTexturePath(const aiMaterial* pMaterial, const std::filesystem::path& modelDirectory)
	{
		if (const auto path = ResolveTexturePath(pMaterial, aiTextureType_BASE_COLOR, modelDirectory))
		{
			return *path;
		}
		if (const auto path = ResolveTexturePath(pMaterial, aiTextureType_DIFFUSE, modelDirectory))
		{
			return *path;
		}
		return {};
	}

	std::filesystem::path ResolveNormalTexturePath(const aiMaterial* pMaterial, const std::filesystem::path& modelDirectory)
	{
		if (const auto path = ResolveTexturePath(pMaterial, aiTextureType_NORMALS, modelDirectory))
		{
			return *path;
		}
		if (const auto path = ResolveTexturePath(pMaterial, aiTextureType_HEIGHT, modelDirectory))
		{
			return *path;
		}
		return {};
	}

namespace dx = DirectX;
dx::XMFLOAT4X4 ConvertMatrix(const aiMatrix4x4& source)
{
	dx::XMFLOAT4X4 result;
	const dx::XMFLOAT4X4 input(source.a1, source.a2, source.a3, source.a4,
		source.b1, source.b2, source.b3, source.b4, source.c1, source.c2, source.c3, source.c4,
		source.d1, source.d2, source.d3, source.d4);
	dx::XMStoreFloat4x4(&result, dx::XMMatrixTranspose(dx::XMLoadFloat4x4(&input)));
	return result;
}
dx::XMFLOAT3 ConvertVector(const aiVector3D& v) { return { v.x, v.y, v.z }; }
void ReadNodes(const aiNode& node, std::uint32_t parent, Animation::Skeleton& skeleton, bool requireSRT)
{
	const auto index = static_cast<std::uint32_t>(skeleton.nodes.size());
	Animation::SkeletonNode result;
	result.name = node.mName.C_Str(); result.parent = parent;
	result.bindLocal = ConvertMatrix(node.mTransformation);
	dx::XMVECTOR s, r, t;
	if (!dx::XMMatrixDecompose(&s, &r, &t, dx::XMLoadFloat4x4(&result.bindLocal)))
	{
		if (requireSRT) throw std::runtime_error("Rig contains a non-SRT transform: " + result.name);
	}
	else
	{
		dx::XMStoreFloat3(&result.scale, s); dx::XMStoreFloat4(&result.rotation, r); dx::XMStoreFloat3(&result.translation, t);
		if (requireSRT && (result.scale.x <= 0 || result.scale.y <= 0 || result.scale.z <= 0))
			throw std::runtime_error("Rig requires positive, nonsingular bind scales: " + result.name);
	}
	if (node.mNumMeshes) result.meshes.assign(node.mMeshes, node.mMeshes + node.mNumMeshes);
	skeleton.nodes.push_back(std::move(result));
	for (unsigned i = 0; i < node.mNumChildren; ++i) ReadNodes(*node.mChildren[i], index, skeleton, requireSRT);
}
const aiScene& Read(Assimp::Importer& importer, const std::filesystem::path& path, bool model)
{
	importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false);
	unsigned flags = aiProcess_ConvertToLeftHanded;
	if (model) flags |= aiProcess_Triangulate | aiProcess_JoinIdenticalVertices |
		aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace;
	const auto scene = importer.ReadFile(path.string(), flags);
	if (!scene || !scene->mRootNode) throw std::runtime_error("Import failed: " + path.string() + ": " + importer.GetErrorString());
	return *scene;
}
DirectX::BoundingBox Bounds(const std::vector<dx::XMFLOAT3>& points)
{
	dx::BoundingBox box = {};
	if (!points.empty()) dx::BoundingBox::CreateFromPoints(box, points.size(), points.data(), sizeof(dx::XMFLOAT3));
	return box;
}
Animation::Interpolation ConvertInterpolation(aiAnimInterpolation interpolation)
{
	if (interpolation == aiAnimInterpolation_Step) return Animation::Interpolation::Step;
	if (interpolation == aiAnimInterpolation_Linear || interpolation == aiAnimInterpolation_Spherical_Linear)
		return Animation::Interpolation::Linear;
	throw std::runtime_error("Unsupported animation curve interpolation (cubic curves must be baked before export).");
}
}

std::shared_ptr<const ModelAsset> ImportModel(const std::filesystem::path& path)
{
	Assimp::Importer importer;
	const auto& scene = Read(importer, path, true);
	if (!scene.mNumMeshes) throw std::runtime_error("Model contains no meshes: " + path.string());
	auto result = std::make_shared<ModelAsset>();
	bool hasSkin = false;
	for (unsigned i = 0; i < scene.mNumMeshes; ++i) hasSkin |= scene.mMeshes[i]->HasBones();
	ReadNodes(*scene.mRootNode, Animation::NoNode, result->skeleton, hasSkin);
	std::unordered_map<std::string, std::uint32_t> nodeIndices;
	for (std::uint32_t i = 0; i < result->skeleton.nodes.size(); ++i)
		if (!nodeIndices.emplace(result->skeleton.nodes[i].name, i).second && hasSkin)
			throw std::runtime_error("Ambiguous model rig node: " + result->skeleton.nodes[i].name);

	for (unsigned meshIndex = 0; meshIndex < scene.mNumMeshes; ++meshIndex)
	{
		const auto& input = *scene.mMeshes[meshIndex];
		if (!input.mNumVertices || !input.mNumFaces) throw std::runtime_error("Empty mesh in model.");
		if (input.mNumBones > Animation::MaxBones) throw std::runtime_error("Mesh exceeds the 128-bone skinning limit: " + std::string(input.mName.C_Str()));
		MeshAsset mesh;
		mesh.name = input.mName.C_Str(); mesh.hasUV = input.HasTextureCoords(0);
		std::vector<std::vector<std::pair<std::uint32_t, float>>> influences(input.mNumVertices);
		for (unsigned b = 0; b < input.mNumBones; ++b)
		{
			const auto& bone = *input.mBones[b];
			const auto found = nodeIndices.find(bone.mName.C_Str());
			if (found == nodeIndices.end()) throw std::runtime_error("Missing bone node: " + std::string(bone.mName.C_Str()));
			mesh.skin.boneNodes.push_back(found->second);
			result->skeleton.boneNodes.push_back(found->second);
			mesh.skin.inverseBinds.push_back(ConvertMatrix(bone.mOffsetMatrix));
			for (unsigned w = 0; w < bone.mNumWeights; ++w)
			{
				const auto& weight = bone.mWeights[w];
				if (weight.mVertexId >= input.mNumVertices) throw std::runtime_error("Bone weight has invalid vertex index.");
				influences[weight.mVertexId].emplace_back(b, weight.mWeight);
			}
		}
		std::size_t discarded = 0;
		std::vector<dx::XMFLOAT3> points, rigidPoints;
		std::vector<std::vector<dx::XMFLOAT3>> bonePoints(input.mNumBones);
		mesh.vertices.reserve(input.mNumVertices);
		for (unsigned v = 0; v < input.mNumVertices; ++v)
		{
			ModelVertex vertex;
			vertex.position = ConvertVector(input.mVertices[v]);
			vertex.normal = input.HasNormals() ? ConvertVector(input.mNormals[v]) : dx::XMFLOAT3{ 0, 1, 0 };
			vertex.tangent = input.HasTangentsAndBitangents() ? ConvertVector(input.mTangents[v]) : dx::XMFLOAT3{ 1, 0, 0 };
			vertex.uv = mesh.hasUV ? dx::XMFLOAT2{ input.mTextureCoords[0][v].x, input.mTextureCoords[0][v].y } : dx::XMFLOAT2{};
			vertex.skin = Animation::PackInfluences(std::move(influences[v]), discarded);
			points.push_back(vertex.position);
			const std::uint32_t ids[] = { vertex.skin.indices.x, vertex.skin.indices.y, vertex.skin.indices.z, vertex.skin.indices.w };
			const float weights[] = { vertex.skin.weights.x, vertex.skin.weights.y, vertex.skin.weights.z, vertex.skin.weights.w };
			if (weights[0] == 0) rigidPoints.push_back(vertex.position);
			for (int k = 0; k < 4; ++k) if (weights[k] > 0) bonePoints[ids[k]].push_back(vertex.position);
			mesh.vertices.push_back(vertex);
		}
		if (discarded) spdlog::warn("{}: discarded {} influences beyond four per vertex", mesh.name, discarded);
		mesh.bounds = Bounds(points);
		mesh.skin.rigidBounds = Bounds(rigidPoints); mesh.skin.hasRigidBounds = !rigidPoints.empty();
		for (const auto& vertices : bonePoints)
		{
			mesh.skin.influenceBounds.push_back(Bounds(vertices));
			mesh.skin.hasInfluenceBounds.push_back(!vertices.empty());
		}
		for (unsigned f = 0; f < input.mNumFaces; ++f)
		{
			const auto& face = input.mFaces[f];
			if (face.mNumIndices != 3) throw std::runtime_error("Model mesh contains non-triangle primitives.");
			for (unsigned k = 0; k < 3; ++k)
			{
				if (face.mIndices[k] >= input.mNumVertices) throw std::runtime_error("Mesh index out of range.");
				mesh.indices.push_back(face.mIndices[k]);
			}
		}
		const aiMaterial* material = input.mMaterialIndex < scene.mNumMaterials ? scene.mMaterials[input.mMaterialIndex] : nullptr;
		mesh.material = LoadMaterialConstants(material);
		if (mesh.hasUV)
		{
			mesh.baseColorTexture = ResolveBaseColorTexturePath(material, path.parent_path());
			mesh.normalTexture = ResolveNormalTexturePath(material, path.parent_path());
		}
		result->meshes.push_back(std::move(mesh));
	}
	auto& bones = result->skeleton.boneNodes;
	std::sort(bones.begin(), bones.end()); bones.erase(std::unique(bones.begin(), bones.end()), bones.end());
	Animation::ValidateSkeleton(result->skeleton);
	return result;
}

std::vector<std::shared_ptr<const Animation::AnimationClip>> ImportAnimations(const std::filesystem::path& path)
{
	Assimp::Importer importer;
	const auto& scene = Read(importer, path, false);
	if (!scene.mNumAnimations) throw std::runtime_error("File contains no animation clips: " + path.string());
	Animation::Skeleton rig;
	ReadNodes(*scene.mRootNode, Animation::NoNode, rig, true);
	std::vector<std::shared_ptr<const Animation::AnimationClip>> result;
	for (unsigned i = 0; i < scene.mNumAnimations; ++i)
	{
		const auto& source = *scene.mAnimations[i];
		auto clip = std::make_shared<Animation::AnimationClip>();
		clip->id = std::filesystem::weakly_canonical(path).generic_string() + "#" + std::to_string(i);
		clip->label = path.stem().string() + " / " + source.mName.C_Str() + " [" + std::to_string(i) + "]";
		clip->sourceRig = rig;
		double ticks = source.mTicksPerSecond;
		if (ticks == 0) { ticks = 30; spdlog::warn("{} has no tick rate; using 30 ticks/second", path.string()); }
		if (!std::isfinite(ticks) || ticks <= 0) throw std::runtime_error("Invalid animation tick rate.");
		clip->duration = source.mDuration / ticks;
		for (unsigned c = 0; c < source.mNumChannels; ++c)
		{
			const auto& channel = *source.mChannels[c];
			Animation::NodeTrack track;
			track.nodeName = channel.mNodeName.C_Str();
			for (unsigned k = 0; k < channel.mNumPositionKeys; ++k)
			{
				const auto& key = channel.mPositionKeys[k];
				track.translations.push_back({ key.mTime / ticks, ConvertVector(key.mValue), ConvertInterpolation(key.mInterpolation) });
			}
			for (unsigned k = 0; k < channel.mNumRotationKeys; ++k)
			{
				const auto& key = channel.mRotationKeys[k];
				track.rotations.push_back({ key.mTime / ticks, {key.mValue.x, key.mValue.y, key.mValue.z, key.mValue.w}, ConvertInterpolation(key.mInterpolation) });
			}
			for (unsigned k = 0; k < channel.mNumScalingKeys; ++k)
			{
				const auto& key = channel.mScalingKeys[k];
				track.scales.push_back({ key.mTime / ticks, ConvertVector(key.mValue), ConvertInterpolation(key.mInterpolation) });
			}
			clip->tracks.push_back(std::move(track));
		}
		if (source.mNumMeshChannels || source.mNumMorphMeshChannels) throw std::runtime_error("Mesh/morph animation is not supported by Animator.");
		Animation::ValidateClip(*clip);
		result.push_back(std::move(clip));
	}
	return result;
}


