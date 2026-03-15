#include "App.h"

App::App():
	wnd (1920, 1080, "TapiEngine v0.4")
{
	// Initialize scene objects
	ResetSimulation();

	wnd.Gfx().SetProjection(DirectX::XMMatrixPerspectiveLH(1.0f, 9.0f / 16.0f, 0.5f, 100.0f));

	// set editor cam starting position
	editorCam.SetPosition(0.0f, 0.0f, -20.0f);
	activeCam = &editorCam;

	imgui.SetContext({
		&scene,
		activeCam,
		&pointLights,
		&isPlayMode,
		&isPaused,
		[this]() { ResetSimulation(); }
	});

	// Mouse cursor start position
	lastMouseX = wnd.mouse.GetPosX();
	lastMouseY = wnd.mouse.GetPosY();
}

App::~App()
{

}

void App::ResetSimulation()
{
	scene.Clear();
	gameCams.clear();
	pointLights.clear();

	std::mt19937 rng(std::random_device{}());
	std::uniform_real_distribution<float> adist(0.0f, 3.1415f * 2.0f);
	std::uniform_real_distribution<float> ddist(0.0f, 3.1415f * 2.0f);
	std::uniform_real_distribution<float> odist(0.0f, 3.1415f * 0.3f);
	std::uniform_real_distribution<float> rdist(6.0f, 20.0f);
	std::uniform_real_distribution<float> bdist{ 0.4f, 3.0f };

	// GO initialization
	auto& cameraObject = scene.CreateGameObject("Camera");
	auto& gameCam = cameraObject.AddComponent<Camera>();
	gameCam.SetPosition(0.0f, 0.0f, 0.0f);

	auto& pointLightObject = scene.CreateGameObject("PointLight");
	pointLightObject.AddComponent<PointLight>(wnd.Gfx());

	auto& root = scene.CreateGameObject("FlyingBoxes");
	for (auto i = 0; i < 120; i++)
	{
		auto& object = scene.CreateChildGameObject(root, "Box " + std::to_string(i));
		object.AddComponent<DrawableComponent>(std::make_unique<Box>(
			wnd.Gfx(), rng, adist,
			ddist, odist, rdist, bdist
		));
	}

	auto& suzanne = scene.CreateGameObject("suzanne");
	suzanne.AddComponent<DrawableComponent>(std::make_unique<Model>(
		wnd.Gfx(),
		"Graphics/Models/suzanne.obj",
		DirectX::XMMatrixScaling(5.0f, 5.0f, 5.0f) * DirectX::XMMatrixTranslation(2.0f, 0.0f, 0.0f)
	));

	CacheSceneComponents();

	if (isPlayMode)
		activeCam = gameCams.empty() ? &editorCam : gameCams.front();
	else activeCam = &editorCam;

	// Test for map generator
	//DungeonGenerator gen(22222);
	//gen.Generate(80, 40);
	//gen.SaveToFile("dungeon2.txt");
}

// Cache pointers to important components (cameras, lights) for easy access during update and rendering
void App::CacheSceneComponents() noexcept
{
	gameCams.clear();
	pointLights.clear();

	auto collect = [&](auto& self, const GameObject& gameObject) -> void
	{
		for (const auto& component : gameObject.GetComponents())
		{
			if (auto camera = dynamic_cast<Camera*>(component.get()))
			{
				gameCams.push_back(camera);
			}
			if (auto pointLight = dynamic_cast<PointLight*>(component.get()))
			{
				pointLights.push_back(pointLight);
			}
		}

		for (const auto& child : gameObject.GetChildren())
		{
			self(self, *child);
		}
	};

	for (const auto& rootObject : scene.GetRootObjects())
	{
		collect(collect, *rootObject);
	}
}

int App::Begin()
{
	// Define timestep and init timer
	timer.Mark();

	// If ecode has value (some event handling)
	while (true) {
		if (const auto ecode = Window::ProcessMessages())
			return *ecode;

		// Accumulate the time elapsed since the last frame
		accumulator += timer.Mark();

		if (needsReset)
		{
			ResetSimulation();
			needsReset = false;
		}

		// As long as we have enough accumulated time,
		// run the update logic in fixed steps.
		while (accumulator >= dt)
		{
			HandleInput(dt);

			// Step Physics
			if (isPlayMode && !isPaused)
			{
				physicsWorld.Update(dt);
			}
			
			scene.Update(dt, isPlayMode && !isPaused);

			accumulator -= dt;
		}

		// alpha represents how far we are between the last physics frame and the next one (0.0 to 1.0)
		const float alpha = accumulator / dt;
		RenderFrame(alpha);
	}
}

// Run per-frame update for rendering
void App::RenderFrame(float alpha)
{
	wnd.Gfx().BeginFrame(0.4f, 0.6f, 0.8f);
	
	if (activeCam != nullptr)
	{
		wnd.Gfx().SetCamera(activeCam->GetViewMatrix());
	}

	for (const auto* light : pointLights)
	{
		if (light != nullptr)
		{
			light->Bind(wnd.Gfx());
		}
	}

	// --- Simulation Draw ---
	scene.Render(wnd.Gfx());
	for (const auto* light : pointLights)
	{
		if (light != nullptr)
		{
			light->Draw(wnd.Gfx());
		}
	}

	// --- UI Logic ---
	imgui.SetContext({
		&scene,
		activeCam,
		&pointLights,
		&isPlayMode,
		&isPaused,
		[this]() { needsReset = true; }
	});
	imgui.EditorWindow();

	wnd.Gfx().Endframe();
}

void App::HandleInput(float dt)
{
	// --- Input Handling & Camera Control ---
	activeCam = &editorCam;
	if (isPlayMode && !gameCams.empty())
	{
		activeCam = gameCams.front();
	}

	const float camSpeed = 12.0f * dt;
	const float rotateSpeed = 0.004f;

	if (activeCam == nullptr)
	{
		return;
	}

	// Mouse Input Polling
	const Mouse::Event e = wnd.mouse.Read();

	if (e.GetType() == Mouse::Event::Type::WheelUp)
		activeCam->Translate({ 0.0f, 0.0f, camSpeed * 3.0f }); // Zoom in (move forward)
	else if (e.GetType() == Mouse::Event::Type::WheelDown)
		activeCam->Translate({ 0.0f, 0.0f, -camSpeed * 3.0f }); // Zoom out (move backward)


	const int curMouseX = wnd.mouse.GetPosX();
	const int curMouseY = wnd.mouse.GetPosY();
	const int mouseDx = curMouseX - lastMouseX;
	const int mouseDy = curMouseY - lastMouseY;

	if (wnd.mouse.IsLeftPressed())
	{
		// Holding left click + moving: standard fps style looking
		activeCam->Rotate((float)mouseDx * rotateSpeed, (float)mouseDy * rotateSpeed);
	}

	if (wnd.mouse.IsMiddlePressed())
	{
		// Holding middle click + moving: move up / down (relative to view, stored in local Y)
		activeCam->Translate({ (float)mouseDx * camSpeed * 0.1f, 0.0f, 0.0f });
		activeCam->Translate({ 0.0f, -(float)mouseDy * camSpeed * 0.1f, 0.0f });
	}

	lastMouseX = curMouseX;
	lastMouseY = curMouseY;

	// Keyboard Input (WASD / Arrows)
	DirectX::XMFLOAT3 translation = { 0.0f, 0.0f, 0.0f };
	if (wnd.kbd.IsKeyPressed('W') || wnd.kbd.IsKeyPressed(VK_UP))
	{
		translation.z += camSpeed;
	}
	if (wnd.kbd.IsKeyPressed('S') || wnd.kbd.IsKeyPressed(VK_DOWN))
	{
		translation.z -= camSpeed;
	}
	if (wnd.kbd.IsKeyPressed('A') || wnd.kbd.IsKeyPressed(VK_LEFT))
	{
		translation.x -= camSpeed;
	}
	if (wnd.kbd.IsKeyPressed('D') || wnd.kbd.IsKeyPressed(VK_RIGHT))
	{
		translation.x += camSpeed;
	}
	activeCam->Translate(translation);

}
