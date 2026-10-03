#pragma once

#include "ModelAsset.h"
#include <cstdint>

// CPU resource data, never part of scene JSON. Shared ownership lets a future
// AssetManager cache resources and release them after their last consumer.
struct TextureAsset
{
	std::vector<std::uint8_t> encodedImage;
};

class AssetLoader
{
public:
	virtual ~AssetLoader() = default;
	virtual std::shared_ptr<const ModelAsset> LoadModel(const std::filesystem::path& path) = 0;
	virtual std::vector<std::shared_ptr<const Animation::AnimationClip>> LoadAnimations(const std::filesystem::path& path) = 0;
	virtual std::shared_ptr<const TextureAsset> LoadTexture(const std::filesystem::path& path) = 0;
	static AssetLoader& Default();
};

class FileAssetLoader final : public AssetLoader
{
public:
	std::shared_ptr<const ModelAsset> LoadModel(const std::filesystem::path& path) override;
	std::vector<std::shared_ptr<const Animation::AnimationClip>> LoadAnimations(const std::filesystem::path& path) override;
	std::shared_ptr<const TextureAsset> LoadTexture(const std::filesystem::path& path) override;
};
