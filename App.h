#pragma once
#include "Window.h"
#include "Tools/Timer.h"
#include "Tools/DungeonGenerator.h"

#include "Graphics/Camera.h"
#include "Graphics/Drawable/Box.h"
#include "Graphics/Drawable/CubeMap.h"
#include "Graphics/Drawable/Ground.h"
#include "Graphics/Drawable/Model.h"
#include "Graphics/Drawable/Primitive.h"
#include "Graphics/Lighting/DirectionalLight.h"
#include "Graphics/Lighting/PointLight.h"
#include "Graphics/Renderer.h"

#include "Physics/PhysicsWorld.h"

#include "imgui/ImguiManager.h"

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
	void CacheSceneComponents() noexcept;
	DirectX::SimpleMath::Ray BuildMouseRay(int mouseX, int mouseY) noexcept;

	void HandleInput(float dt); // handles input per frame
private:
	ImguiManager imgui;		// initializes imgui
	Window wnd;
	Timer timer;
	Scene scene;

	// Cameras for different modes
	Camera editorCam;
	std::vector<Camera*> gameCams;

	Camera* activeCam = nullptr; // reference to currently active camera

	std::vector<PointLight*> pointLights;
	std::vector<DirectionalLight*> directionalLights;
	Renderer renderer;

	// Persistent game objects for inter-scene use (e.g. player character)
	std::vector<GameObject*> persistentObjects;

	// Simulation state
	bool isPlayMode = false; // false = Edit Mode, true = Play Mode
	bool isPaused = false;   // true = Simulation Paused (while in Play Mode)
	bool needsReset = false;  // flag to indicate if simulation needs reset (used to defer reset until end of frame)

	// Input state
	int lastMouseX = 0;
	int lastMouseY = 0;

	// Physics
	Physics::PhysicsWorld physicsWorld;

	const float dt = 1.0f / TARGET_FPS;
	float accumulator = 0.0f;
};
