#pragma once

#include "Guid.h"
#include <filesystem>
#include <unordered_map>

class Graphics;
class GameObject;
class Component;

struct LoadContext
{
	Graphics& graphics;
	std::filesystem::path projectRoot = ".";
	// Populated by the future scene loader for reference resolution.
	std::unordered_map<Guid, GameObject*> gameObjects;
	std::unordered_map<Guid, Component*> components;
};
