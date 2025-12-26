#include "App.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"

App::App():
	wnd (800, 600, "TapiEngine v0.2")
{
	std::mt19937 rng(std::random_device{}());
	std::uniform_real_distribution<float> adist(0.0f, 3.1415f * 2.0f);
	std::uniform_real_distribution<float> ddist(0.0f, 3.1415f * 2.0f);
	std::uniform_real_distribution<float> odist(0.0f, 3.1415f * 0.3f);
	std::uniform_real_distribution<float> rdist(6.0f, 20.0f);
	std::uniform_real_distribution<float> bdist{ 0.4f,3.0f };

	drawables.reserve(nDrawables);

	// Example: Create 8 textured boxes
	for (auto i = 0; i < 24; i++)
	{
		// Create a TexturedBox instead of a Box
		// Ensure the path exists relative to the running executable!
		drawables.push_back(std::make_unique<TexturedBox>(
			wnd.Gfx(), rng, adist,
			ddist, odist, rdist,
			L"Graphics\\Textures\\obama.jpg" // Pass the texture path here
		));
	}

	/*for (auto i = 0; i < 8; i++)
	{
		drawables.push_back(std::make_unique<Box>(
			wnd.Gfx(), rng, adist,
			ddist, odist, rdist, bdist
		));
	}*/

	wnd.Gfx().SetProjection(DirectX::XMMatrixPerspectiveLH(1.0f, 3.0f / 4.0f, 0.5f, 100.0f));
}

App::~App()
{

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
	if (wnd.kbd.IsKeyPressed(VK_TAB))
		wnd.Gfx().EnableImGui();
	else
		wnd.Gfx().DisableImGui();

	wnd.Gfx().BeginFrame(0.4f, 0.2f, 1.0f);
	for (auto& b : drawables)
	{
		b->Update(dt);
		b->Draw(wnd.Gfx());
	}

	// show imgui demo window
	if (wnd.Gfx().IsImGuiEnabled())
	{
		if (showDemoWindow)
			ImGui::ShowDemoWindow(&showDemoWindow);
	}
	
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