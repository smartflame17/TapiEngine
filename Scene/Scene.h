#pragma once

#include <vector>
#include <memory>
#include <string>
#include <cstdint>
#include <DirectXMath.h>
#include "../imgui/imgui.h"

class Graphics;
class GameObject;
class DrawableComponent;
class Drawable;

class Scene
{
public:
	Scene();
	Scene(const std::string& sceneName);
	~Scene();

	GameObject& CreateGameObject(const std::string& name);
	GameObject& CreateChildGameObject(GameObject& parent, const std::string& name);

	void Clear() noexcept;
	void Update(float dt, bool isSimulationRunning) noexcept;
	void Render(Graphics& gfx) const noexcept(!IS_DEBUG);
	void SetSkybox(std::unique_ptr<Drawable> drawable);
	void RegisterDrawable(DrawableComponent* drawable) noexcept;
	void UnregisterDrawable(DrawableComponent* drawable) noexcept;
	void DrawHierarchyWindow() noexcept;
	void DrawInspectorWindow() noexcept;

	const std::vector<std::unique_ptr<GameObject>>& GetRootObjects() const noexcept;
	GameObject* GetSelectedObject() const noexcept;
	DirectX::XMMATRIX GetSelectedWorldTransformMatrix() const noexcept;
	void SetSelectedWorldTransformMatrix(DirectX::FXMMATRIX matrix) noexcept;

private:
	void DrawHierarchyNode(GameObject& object) noexcept;

private:
	std::string name;
	std::vector<std::unique_ptr<GameObject>> rootObjects;
	std::unique_ptr<Drawable> skybox;
	std::vector<DrawableComponent*> drawables;
	GameObject* selectedObject = nullptr;
};
