#include "AssetLoader.h"
#include <fstream>
#include <iterator>
#include <stdexcept>

AssetLoader& AssetLoader::Default()
{
	static FileAssetLoader loader;
	return loader;
}

std::shared_ptr<const ModelAsset> FileAssetLoader::LoadModel(const std::filesystem::path& path)
{
	return ImportModel(path);
}

std::vector<std::shared_ptr<const Animation::AnimationClip>> FileAssetLoader::LoadAnimations(const std::filesystem::path& path)
{
	return ImportAnimations(path);
}

std::shared_ptr<const TextureAsset> FileAssetLoader::LoadTexture(const std::filesystem::path& path)
{
	std::ifstream file(path, std::ios::binary);
	if (!file) throw std::runtime_error("Cannot open texture: " + path.generic_string());
	auto asset = std::make_shared<TextureAsset>();
	asset->encodedImage.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
	if (file.bad() || asset->encodedImage.empty())
		throw std::runtime_error("Cannot read texture: " + path.generic_string());
	return asset;
}
