#pragma once
#include "Window.h"
#include "Tools/Timer.h"
#include "Tools/DungeonGenerator.h"

#include "Graphics/Camera.h"
#include "Graphics/Drawable/Box.h"
#include "Graphics/Drawable/Mesh.h"
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
#define TARGET_FPS 60.0f

class App
{
public:
	App();
	~App();
	int Begin();	// handles message pump between windows and the app
private:
	void Update(float dt);	// called per frame
	void RenderFrame(float alpha); // renders the frame, alpha for physics interpolation
	void ResetSimulation();	// resets camera, light, and all drawables to initial state

	void HandleInput(float dt); // handles input per frame
private:
	ImguiManager imgui;		// initializes imgui
	Window wnd;
	Timer timer;

	// Cameras for different modes
	Camera editorCam;
	Camera gameCam;

	Camera* activeCam = nullptr; // reference to currently active camera

	PointLight light;
	Scene scene;
	static constexpr size_t nDrawables = 180;
	bool showDemoWindow = true;

	// Testing model loading and rendering
	Model suzanne{ wnd.Gfx(), "Graphics/Models/suzanne.obj" };

	// Simulation state
	bool isPlayMode = false; // false = Edit Mode, true = Play Mode
	bool isPaused = false;   // true = Simulation Paused (while in Play Mode)

	// Input state
	int lastMouseX = 0;
	int lastMouseY = 0;

	// Physics
	Physics::PhysicsWorld physicsWorld;

	const float dt = 1.0f / TARGET_FPS;
	float accumulator = 0.0f;
};