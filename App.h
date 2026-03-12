#pragma once
#include "Window.h"
#include "Tools/Timer.h"
#include "Tools/DungeonGenerator.h"

#include "Graphics/Camera.h"
#include "Graphics/Drawable/Box.h"
#include "Graphics/Drawable/Model.h"
#include "Graphics/Lighting/PointLight.h"

#include "Physics/PhysicsWorld.h"

#include "imgui/ImguiManager.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"

#include "Scene/Scene.h"
#include "Scene/GameObject.h"

#include "Components/Component.h"
#include "Components/DrawableComponent.h"
#include <vector>
#include <random>
#define TARGET_FPS 60.0f

class App
{
public:
	App();
	~App();
	int Begin();

private:
	void Update(float dt);
	void RenderFrame(float alpha);
	void ResetSimulation();
	void HandleInput(float dt);
	void RefreshSceneComponentCaches();

private:
	ImguiManager imgui;
	Window wnd;
	Timer timer;

	Camera editorCam;
	std::vector<Camera*> gameCams;
	std::vector<PointLight*> sceneLights;
	Camera* activeCam = nullptr;

	Scene scene;
	static constexpr size_t nDrawables = 180;
	bool showDemoWindow = true;

	bool isPlayMode = false;
	bool isPaused = false;

	int lastMouseX = 0;
	int lastMouseY = 0;

	Physics::PhysicsWorld physicsWorld;

	const float dt = 1.0f / TARGET_FPS;
	float accumulator = 0.0f;
};
