#pragma once

#include "Guid.h"
#include <filesystem>
#include <unordered_map>

class Graphics;
class GameObject;
class Component;
class ComponentRegistry;
class AssetLoader;

struct SerializationContext
{
	std::filesystem::path projectRoot = ".";
	const ComponentRegistry* registry = nullptr;
};

struct LoadContext
{
	Graphics& graphics;
	std::filesystem::path projectRoot = ".";
	// Replaced after a successful load; unchanged when validation/reconstruction fails.
	std::unordered_map<Guid, GameObject*> gameObjects;
	std::unordered_map<Guid, Component*> components;
	AssetLoader* assets = nullptr;
	const ComponentRegistry* registry = nullptr;
	AssetLoader& GetAssetLoader() const;
};
