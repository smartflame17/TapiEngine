#include "App.h"

App::App() :
	wnd(1920, 1080, "TapiEngine v0.4")
{
	ResetSimulation();

	wnd.Gfx().SetProjection(DirectX::XMMatrixPerspectiveLH(1.0f, 9.0f / 16.0f, 0.5f, 100.0f));

	editorCam.SetPosition(0.0f, 0.0f, -20.0f);
	activeCam = &editorCam;

	lastMouseX = wnd.mouse.GetPosX();
	lastMouseY = wnd.mouse.GetPosY();
}

App::~App()
{
}

void App::ResetSimulation()
{
	scene.Clear();

	auto& root = scene.CreateGameObject("Root");
	auto& cameraObject = scene.CreateChildGameObject(root, "Camera");
	auto& gameCamera = cameraObject.AddComponent<Camera>();
	gameCamera.SetPosition(0.0f, 0.0f, 0.0f);

	auto& lightObject = scene.CreateChildGameObject(root, "PointLight");
	lightObject.AddComponent<PointLight>(wnd.Gfx());

	std::mt19937 rng(std::random_device{}());
	std::uniform_real_distribution<float> adist(0.0f, 3.1415f * 2.0f);
	std::uniform_real_distribution<float> ddist(0.0f, 3.1415f * 2.0f);
	std::uniform_real_distribution<float> odist(0.0f, 3.1415f * 0.3f);
	std::uniform_real_distribution<float> rdist(6.0f, 20.0f);
	std::uniform_real_distribution<float> bdist{ 0.4f, 3.0f };

	for (auto i = 0; i < 120; i++)
	{
		auto& object = scene.CreateChildGameObject(root, "Box " + std::to_string(i));
		object.AddComponent<DrawableComponent>(std::make_unique<Box>(
			wnd.Gfx(), rng, adist,
			ddist, odist, rdist, bdist
		));
	}

	auto& suzanne = scene.CreateChildGameObject(root, "suzanne");
	suzanne.AddComponent<DrawableComponent>(std::make_unique<Model>(
		wnd.Gfx(),
		"Graphics/Models/suzanne.obj",
		DirectX::XMMatrixScaling(5.0f, 5.0f, 5.0f) * DirectX::XMMatrixTranslation(2.0f, 0.0f, 0.0f)
	));

	RefreshSceneComponentCaches();
}

void App::RefreshSceneComponentCaches()
{
	gameCams.clear();
	sceneLights.clear();

	auto collectComponents = [&](GameObject& gameObject, auto&& collectRef) -> void
	{
		for (auto& component : gameObject.GetComponents())
		{
			if (auto* camera = dynamic_cast<Camera*>(component.get()))
			{
				gameCams.push_back(camera);
			}
			if (auto* light = dynamic_cast<PointLight*>(component.get()))
			{
				sceneLights.push_back(light);
			}
		}

		for (auto& child : gameObject.GetChildren())
		{
			collectRef(*child, collectRef);
		}
	};

	for (auto& rootObject : scene.GetRootObjects())
	{
		collectComponents(*rootObject, collectComponents);
	}
}

int App::Begin()
{
	timer.Mark();

	while (true) {
		if (const auto ecode = Window::ProcessMessages())
			return *ecode;

		accumulator += timer.Mark();

		while (accumulator >= dt)
		{
			HandleInput(dt);

			if (isPlayMode && !isPaused)
			{
				physicsWorld.Update(dt);
			}

			scene.Update(dt, isPlayMode && !isPaused);

			accumulator -= dt;
		}

		const float alpha = accumulator / dt;
		RenderFrame(alpha);
	}
}

void App::RenderFrame(float alpha)
{
	wnd.Gfx().BeginFrame(0.4f, 0.6f, 0.8f);

	wnd.Gfx().SetCamera(activeCam->GetViewMatrix());

	for (const auto* light : sceneLights)
	{
		light->Bind(wnd.Gfx());
	}

	scene.Render(wnd.Gfx());

	ImGui::PushStyleVar(ImGuiStyleVar_WindowTitleAlign, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(1280, 60), ImGuiCond_Always);
	ImGui::SetNextWindowPos(ImVec2(300, 0), ImGuiCond_Always);
	if (ImGui::Begin("TapiEngine v0.4", nullptr,
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoCollapse
	))
	{
		const char* playBtnLabel = isPlayMode ? (isPaused ? "Resume" : "Pause") : "Play";
		const char* stopBtnLabel = "Stop";
		float width1 = ImGui::CalcTextSize(playBtnLabel).x + ImGui::GetStyle().FramePadding.x * 2.0f;
		float width2 = ImGui::CalcTextSize(stopBtnLabel).x + ImGui::GetStyle().FramePadding.x * 2.0f;
		float spacing = ImGui::GetStyle().ItemSpacing.x;

		float totalWidth = width1 + width2 + spacing;
		float windowWidth = ImGui::GetContentRegionAvail().x;

		float indentation = (windowWidth - totalWidth) * 0.5f;

		if (indentation > 0.0f) {
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indentation);
		}

		if (ImGui::Button(playBtnLabel))
		{
			if (!isPlayMode)
			{
				isPlayMode = true;
				isPaused = false;
			}
			else
			{
				isPaused = !isPaused;
			}
		}

		ImGui::SameLine();
		ImGui::BeginDisabled(!isPlayMode);
		if (ImGui::Button(stopBtnLabel))
		{
			if (isPlayMode)
			{
				isPlayMode = false;
				isPaused = false;
				ResetSimulation();
			}
		}
		ImGui::EndDisabled();
	}
	ImGui::End();
	ImGui::PopStyleVar();

	scene.DrawHierarchyWindow();

	imgui.StatWindow();
	activeCam->SpawnControlWindow();
	for (auto* light : sceneLights)
	{
		light->SpawnControlWindow();
	}

	ImGui::ShowDemoWindow(nullptr);

	wnd.Gfx().Endframe();
}

void App::HandleInput(float dt)
{
	if (isPlayMode && !gameCams.empty())
	{
		activeCam = gameCams.front();
	}
	else
	{
		activeCam = &editorCam;
	}

	const float camSpeed = 12.0f * dt;
	const float rotateSpeed = 0.004f;

	const Mouse::Event e = wnd.mouse.Read();

	if (e.GetType() == Mouse::Event::Type::WheelUp)
		activeCam->Translate({ 0.0f, 0.0f, camSpeed * 3.0f });
	else if (e.GetType() == Mouse::Event::Type::WheelDown)
		activeCam->Translate({ 0.0f, 0.0f, -camSpeed * 3.0f });

	const int curMouseX = wnd.mouse.GetPosX();
	const int curMouseY = wnd.mouse.GetPosY();
	const int mouseDx = curMouseX - lastMouseX;
	const int mouseDy = curMouseY - lastMouseY;

	if (wnd.mouse.IsLeftPressed())
	{
		activeCam->Rotate((float)mouseDx * rotateSpeed, (float)mouseDy * rotateSpeed);
	}

	if (wnd.mouse.IsMiddlePressed())
	{
		activeCam->Translate({ (float)mouseDx * camSpeed * 0.1f, 0.0f, 0.0f });
		activeCam->Translate({ 0.0f, -(float)mouseDy * camSpeed * 0.1f, 0.0f });
	}

	lastMouseX = curMouseX;
	lastMouseY = curMouseY;

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
