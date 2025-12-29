#pragma once
#include "Window.h"
#include "Tools/Timer.h"
#include "Graphics/Camera.h"
#include "Graphics/Drawable/Box.h"
#include "Graphics/Drawable/TexturedBox.h"
#include "Graphics/Lighting/PointLight.h"
#include "imgui/ImguiManager.h"
#define TARGET_FPS 120.0f

class App
{
public:
	App();
	~App();
	int Begin();	// handles message pump between windows and the app
private:
	void Update(float dt);	// called per frame
	void ResetSimulation();	// resets camera, light, and all drawables to initial state
private:
	ImguiManager imgui;		// initializes imgui
	Window wnd;
	Timer timer;

	Camera cam;
	// Cameras for different modes
	Camera editorCam;
	Camera gameCam;

	PointLight light;
	std::vector<std::unique_ptr<class Drawable>> drawables;
	static constexpr size_t nDrawables = 180;
	bool showDemoWindow = true;

	// Simulation State
	bool isPlayMode = false; // false = Edit Mode, true = Play Mode
	bool isPaused = false;   // true = Simulation Paused (while in Play Mode)
};