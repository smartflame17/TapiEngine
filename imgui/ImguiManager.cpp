#include "ImguiManager.h"
#include "imgui.h"

ImguiManager::ImguiManager()
{
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
}

ImguiManager::~ImguiManager()
{
	ImGui::DestroyContext();
}

void ImguiManager::StatWindow(bool* p_open)
{
	ImGui::Begin("Statistics", p_open);
	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
	ImGui::End();
}