#include "App.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"

App::App():
	wnd (1280, 720, "TapiEngine v0.4"),
	light(wnd.Gfx())
{
	// Initialize scene objects
	ResetSimulation();

	wnd.Gfx().SetProjection(DirectX::XMMatrixPerspectiveLH(1.0f, 9.0f / 16.0f, 0.5f, 100.0f));

	// set editor cam starting position
	editorCam.SetPosition(0.0f, 0.0f, -20.0f);
	activeCam = &editorCam;

	// Mouse cursor start position
	lastMouseX = wnd.mouse.GetPosX();
	lastMouseY = wnd.mouse.GetPosY();
}

App::~App()
{

}

void App::ResetSimulation()
{
	drawables.clear();
	drawables.reserve(nDrawables);

	std::mt19937 rng(std::random_device{}());
	std::uniform_real_distribution<float> adist(0.0f, 3.1415f * 2.0f);
	std::uniform_real_distribution<float> ddist(0.0f, 3.1415f * 2.0f);
	std::uniform_real_distribution<float> odist(0.0f, 3.1415f * 0.3f);
	std::uniform_real_distribution<float> rdist(6.0f, 20.0f);
	std::uniform_real_distribution<float> bdist{ 0.4f, 3.0f };

	for (auto i = 0; i < 120; i++)
	{
		drawables.push_back(std::make_unique<Box>(
			wnd.Gfx(), rng, adist,
			ddist, odist, rdist, bdist
		));
	}

	gameCam.SetPosition(0.0f, 0.0f, 0.0f);

	// Test for map generator
	//DungeonGenerator gen(22222);
	//gen.Generate(80, 40);
	//gen.SaveToFile("dungeon2.txt");
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
			
			// Additional Game logic here...

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
	wnd.Gfx().BeginFrame(0.3f, 0.2f, 0.4f);
	
	wnd.Gfx().SetCamera(activeCam->GetViewMatrix());
	light.Bind(wnd.Gfx());

	// --- Simulation Update & Draw ---
	for (size_t i = 0; i < drawables.size(); i++)
	{
		// Assuming your drawables or game objects hold a reference to a RigidBody
		// auto& rb = myGameObjects[i].rigidBody;

		// Get the SMOOTH interpolated transform
		// Matrix transform = rb->GetInterpolatedTransform(alpha);
		if (isPlayMode && !isPaused)
			drawables[i]->Update(dt); // Update logic if needed

		// Pass this transform to your drawable's Draw method
		drawables[i]->Draw(wnd.Gfx());
		
	}
	suzanne.Draw(wnd.Gfx(), DirectX::XMMatrixScaling(5.0f, 5.0f, 5.0f) * DirectX::XMMatrixTranslation(2.0f, 0.0f, 0.0f));
	light.Draw(wnd.Gfx());

	// --- UI Logic ---
	if (ImGui::Begin("Simulation Control"))
	{
		// Play/Pause Button
		const char* playBtnLabel = isPlayMode ? (isPaused ? "Resume" : "Pause") : "Play";
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
		// Stop Button
		ImGui::BeginDisabled(!isPlayMode);
		if (ImGui::Button("Stop"))
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

	// show imgui
	imgui.StatWindow();
	activeCam->SpawnControlWindow();
	light.SpawnControlWindow();
	suzanne.SpawnControlWindow();

	/*
	// Draw Sprites and Text
	wnd.Gfx().pSpriteBatch->Begin(DirectX::SpriteSortMode_Deferred, // Or your preferred sort mode
		nullptr,                          // Use default BlendState (alpha blend)
		nullptr,                          // Use default SamplerState
		wnd.Gfx().GetDepthStencilState(), // Use Depth State
		nullptr                           // Use default RasterizerState
	);
	wnd.Gfx().pSpriteFont->DrawString(wnd.Gfx().pSpriteBatch.get(), L"Hello, DirectXTK!", DirectX::XMFLOAT2(300, 400), DirectX::Colors::PaleVioletRed);
	wnd.Gfx().pSpriteBatch->End();
	*/
	wnd.Gfx().Endframe();
}

void App::HandleInput(float dt)
{
	// --- Input Handling & Camera Control ---
	activeCam = isPlayMode ? &gameCam : &editorCam;
	const float camSpeed = 12.0f * dt;
	const float rotateSpeed = 0.004f;

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