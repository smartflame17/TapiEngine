#pragma once
#include "Window.h"
#include "Tools/Timer.h"
#include "Graphics/Camera.h"
#include "Graphics/Drawable/Box.h"
#include "Graphics/Drawable/TexturedBox.h"
#include "imgui/ImguiManager.h"
#define TARGET_FPS 60.0f

class App
{
public:
	App();
	~App();
	int Begin();	// handles message pump between windows and the app
private:
	void Update(float dt);	// called per frame
private:
	ImguiManager imgui;		// initializes imgui manager
	Window wnd;
	Timer timer;
	Camera cam;
	std::vector<std::unique_ptr<class Drawable>> drawables;
	static constexpr size_t nDrawables = 180;
	bool showDemoWindow = true;
};