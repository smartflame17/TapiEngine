#include "App.h"

App::Config::Config()
{
	width = GetPrivateProfileIntA("Settings", "Width", 1920, ".\\config.ini");
	height = GetPrivateProfileIntA("Settings", "Height", 1080, ".\\config.ini");

	char buffer[256];
	GetPrivateProfileStringA("Settings", "Version", "TapiEngine v??", buffer, sizeof(buffer), ".\\config.ini");
	title = buffer;
}

App::App():
	config(),
	wnd (config.width, config.height, config.title.c_str()),
	renderer(wnd.Gfx())
{
	// Initialize scene objects
	ResetSimulation();

	// set projection matrix from config
	float aspectRatio = static_cast<float>(config.height) / static_cast<float>(config.width);
	wnd.Gfx().SetProjection(DirectX::XMMatrixPerspectiveLH(1.0f, aspectRatio, 0.5f, 100.0f));

	// set editor cam starting position
	editorCam.SetPosition(0.0f, 2.0f, -5.0f);
	activeCam = &editorCam;
	
	// send state to imgui for display and interaction
	imgui.SetContext({
		&scene,
		activeCam,
		&wnd.Gfx(),
		&pointLights,
		&spotLights,
		&directionalLights,
		&wnd.mouse,
		&isPlayMode,
		&isPaused,
		[this]() { ResetSimulation(); }
	});

	// Mouse cursor start position
	lastMouseX = wnd.mouse.GetPosX();
	lastMouseY = wnd.mouse.GetPosY();

	wnd.DisableCursor();	// disable OS cursor, we'll handle it ourselves for better control in 3D space
}

App::~App()
{

}

void App::ResetSimulation()
{
	scene.Clear();
	gameCams.clear();
	pointLights.clear();
	spotLights.clear();

	std::mt19937 rng(std::random_device{}());

	scene.SetSkybox(std::make_unique<CubeMap>(
		wnd.Gfx(),
		"Graphics/Textures/Skybox/WaterMountain"
	));

	// GO initialization
	auto& cameraObject = scene.CreateGameObject("Camera");
	auto& gameCam = cameraObject.AddComponent<Camera>();
	cameraObject.SetPosition(0.0f, 2.0f, -5.0f);

	auto& pointLightObject = scene.CreateGameObject("PointLight");
	pointLightObject.AddComponent<PointLight>(wnd.Gfx());
	pointLightObject.SetPosition(0.0f, 4.0f, -2.0f);

	auto& spotLightObject = scene.CreateGameObject("SpotLight");
	spotLightObject.AddComponent<SpotLight>(wnd.Gfx());
	spotLightObject.SetPosition(-3.0f, 5.0f, -3.0f);
	spotLightObject.SetRotation(0.9f, 0.75f, 0.0f);

	auto& directionalLightObject = scene.CreateGameObject("DirectionalLight");
	directionalLightObject.AddComponent<DirectionalLight>(wnd.Gfx());
	directionalLightObject.SetRotation(0.4f, -0.7f, 0.0f);
	directionalLightObject.GetComponent<DirectionalLight>()->SetIntensity(0.4f);

	auto& groundObject = scene.CreateGameObject("Ground");
	groundObject.AddComponent<DrawableComponent>(std::make_unique<Ground>(wnd.Gfx()));

	auto& texturedCube = scene.CreateGameObject("Textured Cube");
	texturedCube.AddComponent<DrawableComponent>(std::make_unique<Primitive>(
		wnd.Gfx(),
		Primitive::Shape::Cube,
		Primitive::SurfaceMode::Textured,
		"Graphics/Textures/obama.jpg"
	));
	texturedCube.SetPosition(-1.5f, 1.0f, 0.0f);

	auto& materialCube = scene.CreateGameObject("Material Cube");
	materialCube.AddComponent<DrawableComponent>(std::make_unique<Primitive>(
		wnd.Gfx(),
		Primitive::Shape::Cube,
		Primitive::SurfaceMode::Material
	));
	materialCube.SetPosition(1.5f, 1.0f, 0.0f);

	/*auto& zhu = scene.CreateGameObject("zhu");
	zhu.AddComponent<DrawableComponent>(std::make_unique<Model>(
		wnd.Gfx(),
		"Graphics/Models/2b_nier_automata/scene.gltf"
	));
	zhu.SetPosition(1.0f, 0.0f, 0.0f);
	zhu.SetScale(10.0f, 10.0f, 10.0f);*/

	CacheSceneComponents();

	if (isPlayMode)
		activeCam = gameCams.empty() ? &editorCam : gameCams.front();
	else activeCam = &editorCam;

	// Test for map generator
	//DungeonGenerator gen(22222);
	//gen.Generate(80, 40);
	//gen.SaveToFile("dungeon2.txt");

	spdlog::info("Simulation reset to initial state.");
}

// Cache pointers to important components (cameras, lights) for easy access during update and rendering
void App::CacheSceneComponents() noexcept
{
	gameCams.clear();
	pointLights.clear();
	spotLights.clear();
	directionalLights.clear();

	auto collect = [&](auto& self, const GameObject& gameObject) -> void
	{
		if (gameObject.IsPendingKill())
		{
			return;
		}

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
			if (auto spotLight = dynamic_cast<SpotLight*>(component.get()))
			{
				spotLights.push_back(spotLight);
			}
			if (auto directionalLight = dynamic_cast<DirectionalLight*>(component.get()))
			{
				directionalLights.push_back(directionalLight);
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

		if (isPlayMode) wnd.DisableCursor();
		else wnd.EnableCursor();

		HandleInput(dt);

		// As long as we have enough accumulated time,
		// run the update logic in fixed steps.
		while (accumulator >= dt)
		{
			const bool isSimulationRunning = isPlayMode && !isPaused;

			scene.ProcessScriptAwakeAndStart(isSimulationRunning);

			// Step Physics
			if (isSimulationRunning)
			{
				physicsWorld.Update(dt);
			}

			scene.FixedUpdate(isSimulationRunning);
			scene.Update(dt, isSimulationRunning);
			scene.LateUpdate(dt, isSimulationRunning);

			accumulator -= dt;
		}

		// alpha represents how far we are between the last physics frame and the next one (0.0 to 1.0)
		const float alpha = accumulator / dt;
		RenderFrame(alpha);
		scene.CleanupDestroyedObjects();
		CacheSceneComponents();
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

	// --- Simulation Draw ---
	renderer.Render(scene, activeCam);

	// --- UI Logic ---
	imgui.SetContext({
		&scene,
		activeCam,
		&wnd.Gfx(),
		&pointLights,
		&spotLights,
		&directionalLights,
		&wnd.mouse,
		&isPlayMode,
		&isPaused,
		[this]() { needsReset = true; }
	});
	imgui.EditorWindow();

	wnd.Gfx().Endframe();
}

DirectX::SimpleMath::Ray App::BuildMouseRay(int mouseX, int mouseY) noexcept
{
	namespace dx = DirectX;

	const auto nearPoint = dx::XMVector3Unproject(
		dx::XMVectorSet(static_cast<float>(mouseX), static_cast<float>(mouseY), 0.0f, 1.0f),
		wnd.Gfx().GetViewportTopLeftX(),
		wnd.Gfx().GetViewportTopLeftY(),
		wnd.Gfx().GetViewportWidth(),
		wnd.Gfx().GetViewportHeight(),
		0.0f,
		1.0f,
		wnd.Gfx().GetProjection(),
		activeCam->GetViewMatrix(),
		dx::XMMatrixIdentity()
	);

	const auto farPoint = dx::XMVector3Unproject(
		dx::XMVectorSet(static_cast<float>(mouseX), static_cast<float>(mouseY), 1.0f, 1.0f),
		wnd.Gfx().GetViewportTopLeftX(),
		wnd.Gfx().GetViewportTopLeftY(),
		wnd.Gfx().GetViewportWidth(),
		wnd.Gfx().GetViewportHeight(),
		0.0f,
		1.0f,
		wnd.Gfx().GetProjection(),
		activeCam->GetViewMatrix(),
		dx::XMMatrixIdentity()
	);

	dx::XMFLOAT3 origin;
	dx::XMFLOAT3 direction;
	dx::XMStoreFloat3(&origin, nearPoint);
	dx::XMStoreFloat3(&direction, dx::XMVector3Normalize(dx::XMVectorSubtract(farPoint, nearPoint)));

	std::cout << "[DEBUG] Mouse Ray Origin: (" << origin.x << ", " << origin.y << ", " << origin.z << ")\n";
	std::cout << "[DEBUG] Mouse Ray Direction: (" << direction.x << ", " << direction.y << ", " << direction.z << ")\n";
	return { origin, direction };
}

void App::HandleInput(float dt)
{
	// --- Input Handling & Camera Control ---
	activeCam = &editorCam;
	if (isPlayMode && !gameCams.empty())
	{
		for (auto* camera : gameCams)
		{
			if (camera != nullptr && !camera->GetGameObject().IsPendingKill())
			{
				activeCam = camera;
				break;
			}
		}
	}

	if (activeCam == nullptr)
	{
		return;
	}

	// Mouse Input Polling
	bool leftClicked = false;
	int clickX = 0;
	int clickY = 0;
	Mouse::Event e;
	while ((e = wnd.mouse.Read()).IsValid())
	{
		if (e.GetType() == Mouse::Event::Type::WheelUp)
		{
			activeCam->Translate({ 0.0f, 0.0f, activeCam->camSpeed * 3.0f }); // Zoom in (move forward)
		}
		else if (e.GetType() == Mouse::Event::Type::WheelDown)
		{
			activeCam->Translate({ 0.0f, 0.0f, -activeCam->camSpeed * 3.0f }); // Zoom out (move backward)
		}
		else if (!isPlayMode && e.GetType() == Mouse::Event::Type::LPressed)
		{
			leftClicked = true;
			clickX = e.GetPosX();
			clickY = e.GetPosY();
		}
	}


	const int curMouseX = wnd.mouse.GetPosX();
	const int curMouseY = wnd.mouse.GetPosY();
	int mouseDx = 0;
	int mouseDy = 0;

	if (wnd.mouse.RawEnabled())
	{
		while (const auto delta = wnd.mouse.ReadRawDelta())
		{
			mouseDx += delta->x;
			mouseDy += delta->y;
		}
	}
	else
	{
		mouseDx = curMouseX - lastMouseX;
		mouseDy = curMouseY - lastMouseY;
	}

	if (!isPlayMode)
	{
		if (wnd.mouse.IsRightPressed())
		{
			// Holding right click + moving: standard fps style looking (in editor mode only)
			activeCam->Rotate((float)mouseDx * activeCam->rotateSpeed, (float)mouseDy * activeCam->rotateSpeed);
		}
	}
	else
	{
		// in play mode, no press required for fps-style looking, just mouse movement
		activeCam->Rotate((float)mouseDx * activeCam->rotateSpeed, (float)mouseDy * activeCam->rotateSpeed);
	}

	if (wnd.mouse.IsMiddlePressed())
	{
		// Holding middle click + moving: move up / down (relative to view, stored in local Y)
		activeCam->Translate({ (float)mouseDx * activeCam->camSpeed * 0.1f, 0.0f, 0.0f });
		activeCam->Translate({ 0.0f, -(float)mouseDy * activeCam->camSpeed * 0.1f, 0.0f });
	}

	if (!wnd.mouse.RawEnabled() && isPlayMode)
	{
		wnd.RecenterCursor();
		lastMouseX = wnd.mouse.GetPosX();
		lastMouseY = wnd.mouse.GetPosY();
	}
	else
	{
		lastMouseX = curMouseX;
		lastMouseY = curMouseY;
	}

	if (leftClicked)
	{
		scene.SelectGameObjectByRay(BuildMouseRay(clickX, clickY));
	}

	// Keyboard Input (WASD / Arrows)
	DirectX::XMFLOAT3 translation = { 0.0f, 0.0f, 0.0f };
	if (wnd.kbd.IsKeyPressed('W') || wnd.kbd.IsKeyPressed(VK_UP))
	{
		translation.z += activeCam->camSpeed;
	}
	if (wnd.kbd.IsKeyPressed('S') || wnd.kbd.IsKeyPressed(VK_DOWN))
	{
		translation.z -= activeCam->camSpeed;
	}
	if (wnd.kbd.IsKeyPressed('A') || wnd.kbd.IsKeyPressed(VK_LEFT))
	{
		translation.x -= activeCam->camSpeed;
	}
	if (wnd.kbd.IsKeyPressed('D') || wnd.kbd.IsKeyPressed(VK_RIGHT))
	{
		translation.x += activeCam->camSpeed;
	}
	activeCam->Translate(translation);

	if (wnd.kbd.IsKeyPressed(VK_ESCAPE))
	{
		if (isPlayMode)
		{
			isPlayMode = false;
			ResetSimulation();
		}
	}
}
