#pragma once

#include <vector>
#include <memory>
#include <string>
#include <cstdint>
#include "GameObject.h"
#include "../imgui/imgui.h"

class Graphics;
class GameObject;

class Scene
{
public:
	Scene() : name("Scene") {}
	Scene(const std::string& sceneName) : name(sceneName) {}

	GameObject& CreateGameObject(const std::string& name);
	GameObject& CreateChildGameObject(GameObject& parent, const std::string& name);

	void Clear() noexcept;
	void Update(float dt, bool isSimulationRunning) noexcept;
	void Render(Graphics& gfx) const noexcept(!IS_DEBUG);
	void DrawHierarchyWindow() noexcept;

	const std::vector<std::unique_ptr<GameObject>>& GetRootObjects() const noexcept;

private:
	void DrawHierarchyNode(GameObject& object) noexcept;

private:
	std::string name;
	std::vector<std::unique_ptr<GameObject>> rootObjects;
	GameObject* selectedObject = nullptr;
};
