#include "ImguiManager.h"
ImguiManager::ImguiManager()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
}

ImguiManager::~ImguiManager()
{
	ImGui::DestroyContext();
}

void ImguiManager::SetContext(UiContext context) noexcept
{
	this->context = std::move(context);
}

void ImguiManager::StatWindow(bool* p_open)
{
	ImGui::SetNextWindowSize(ImVec2(300, 60), ImGuiCond_Always);
	ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
	ImGui::Begin("Statistics", p_open,
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoBringToFrontOnFocus
	);
	ImGui::Text("App avg %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
	ImGui::End();

	ImGui::PushStyleVar(ImGuiStyleVar_WindowTitleAlign, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(1280, 60), ImGuiCond_Always);
	ImGui::SetNextWindowPos(ImVec2(300, 0), ImGuiCond_Always);
	if (ImGui::Begin("TapiEngine v0.4", nullptr,
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoBringToFrontOnFocus
	))
	{
		if (context.isPlayMode != nullptr && context.isPaused != nullptr)
		{
			const char* playBtnLabel = *context.isPlayMode ? (*context.isPaused ? "Resume" : "Pause") : "Play";
			const char* stopBtnLabel = "Stop";
			float width1 = ImGui::CalcTextSize(playBtnLabel).x + ImGui::GetStyle().FramePadding.x * 2.0f;
			float width2 = ImGui::CalcTextSize(stopBtnLabel).x + ImGui::GetStyle().FramePadding.x * 2.0f;
			float spacing = ImGui::GetStyle().ItemSpacing.x;

			float totalWidth = width1 + width2 + spacing;
			float windowWidth = ImGui::GetContentRegionAvail().x;
			float indentation = (windowWidth - totalWidth) * 0.5f;
			if (indentation > 0.0f)
			{
				ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indentation);
			}

			if (ImGui::Button(playBtnLabel))
			{
				if (!*context.isPlayMode)
				{
					*context.isPlayMode = true;
					*context.isPaused = false;
				}
				else
				{
					*context.isPaused = !*context.isPaused;
				}
			}

			ImGui::SameLine();
			ImGui::BeginDisabled(!*context.isPlayMode);
			if (ImGui::Button(stopBtnLabel))
			{
				if (*context.isPlayMode)
				{
					*context.isPlayMode = false;
					*context.isPaused = false;
					if (context.resetSimulation)
					{
						context.resetSimulation();
					}
				}
			}
			ImGui::EndDisabled();
		}
	}
	ImGui::End();
	ImGui::PopStyleVar();

	if (context.scene != nullptr)
	{
		context.scene->DrawHierarchyWindow();
		context.scene->DrawInspectorWindow();
	}

	if (context.activeCamera != nullptr)
	{
		context.activeCamera->SpawnControlWindow();
	}

	if (context.pointLights != nullptr)
	{
		for (auto* light : *context.pointLights)
		{
			if (light != nullptr)
			{
				light->SpawnControlWindow();
			}
		}
	}
}
