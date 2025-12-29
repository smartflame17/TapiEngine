#include "App.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"

App::App():
	wnd (1280, 720, "TapiEngine v0.3"),
	light(wnd.Gfx())
{
	// Initialize scene objects
	ResetSimulation();

	wnd.Gfx().SetProjection(DirectX::XMMatrixPerspectiveLH(1.0f, 9.0f / 16.0f, 0.5f, 100.0f));

	// set distinct starting positions so the switch is obvious
	editorCam.SetPosition(0.0f, 0.0f, -20.0f);
	gameCam.SetPosition(0.0f, 0.0f, 0.0f);
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
}

int App::Begin()
{
	// Define timestep and init timer
	const float dt = 1.0f / TARGET_FPS;
	float accumulator = 0.0f;
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
			Update(dt);
			accumulator -= dt;
		}
	}
}

// Run per-frame update
void App::Update(float dt)
{
	wnd.Gfx().BeginFrame(0.3f, 0.2f, 0.4f);
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

	// --- Mode & Camera Logic ---
	// Select the active camera based on mode
	Camera& activeCam = isPlayMode ? gameCam : editorCam;

	wnd.Gfx().SetCamera(activeCam.GetViewMatrix());
	light.Bind(wnd.Gfx());

	// --- Simulation Update & Draw ---
	for (auto& b : drawables)
	{
		if (isPlayMode && !isPaused)
			b->Update(dt);
		
		b->Draw(wnd.Gfx());
	}
	light.Draw(wnd.Gfx());

	// show imgui
	imgui.StatWindow();
	activeCam.SpawnControlWindow();
	light.SpawnControlWindow();
	

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