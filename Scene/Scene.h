#pragma once

#include <vector>
#include <memory>
#include <string>
#include <cstdint>
#include <DirectXMath.h>
#include "../imgui/imgui.h"
#include "../Graphics/Lighting/RenderLight.h"
#include "BVHManager.h"
#include "ScriptManager.h"

class Graphics;
class GameObject;
class DrawableComponent;
class Drawable;
class Camera;
class CustomBehaviour;
class RenderQueueBuilder;
struct RenderView;

class Scene
{
public:
	Scene();
	Scene(const std::string& sceneName);
	~Scene();

	GameObject& CreateGameObject(const std::string& name);
	GameObject& CreateChildGameObject(GameObject& parent, const std::string& name);

	void Clear() noexcept;
	void ProcessScriptAwakeAndStart(bool isSimulationRunning) noexcept;
	void FixedUpdate(bool isSimulationRunning) noexcept;
	void Update(float dt, bool isSimulationRunning) noexcept;
	void Submit(RenderQueueBuilder& queueBuilder, const RenderView& view) noexcept(!IS_DEBUG);
	void LateUpdate(float dt, bool isSimulationRunning) noexcept;
	void CleanupDestroyedObjects() noexcept;
	void SetSkybox(std::unique_ptr<Drawable> drawable);
	void RegisterDrawable(DrawableComponent* drawable) noexcept;
	void UnregisterDrawable(DrawableComponent* drawable) noexcept;
	void RegisterScript(CustomBehaviour& script) noexcept;
	void HandleScriptEnableStateChanged(CustomBehaviour& script) noexcept;
	void DestroyGameObject(GameObject& object) noexcept;
	void DrawHierarchyWindow() noexcept;
	void DrawInspectorWindow() noexcept;
	void SelectGameObjectByRay(const DirectX::SimpleMath::Ray& ray) noexcept;
	void CollectRenderLights(std::vector<RenderLight>& lights) const noexcept;

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
	BVHManager bvhManager;
	ScriptManager scriptManager;
	GameObject* selectedObject = nullptr;
};
