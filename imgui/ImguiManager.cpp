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
	// Top-left corner window and title bar for app statistics and play/pause/stop controls
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

	// Mid-bottom multipurpose window for logging, file browser, etc.
	ImGui::SetNextWindowSize(ImVec2(1280, 300), ImGuiCond_Always);
	ImGui::SetNextWindowPos(ImVec2(300, 780), ImGuiCond_Always);
	ImGui::Begin("Multipurpose", nullptr,
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoTitleBar
	);
	if (ImGui::BeginTabBar("MyTabBar"))
	{
		if (ImGui::BeginTabItem("Log"))
		{
			ImGui::TextUnformatted("This is a log window. Redirect application's log output here.");
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Asset Browser"))
		{
			ImGui::TextUnformatted("This is a asset browser. Implement file loading/saving functionality here.");
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
	ImGui::End();

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

	// Top-most title menu skeleton (for future use, e.g. file/edit/view menus)
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::BeginMenu("New"))
			{
				if (ImGui::BeginMenu("Scene"))
				{
					if (ImGui::MenuItem("Empty Scene")) {}
					if (ImGui::MenuItem("Default Scene")) {}
					ImGui::EndMenu();
				}
				ImGui::Separator();
				if (ImGui::BeginMenu("GameObject"))
				{
					if (ImGui::MenuItem("Empty GameObject")) {}
					if (ImGui::MenuItem("Camera")) {}
					if (ImGui::MenuItem("Light")) {}
					if (ImGui::MenuItem("Model")) 
					{
						fileDialog.Open();
						fileDialog.Display();
						if (fileDialog.HasSelected())
						{
							std::cout << fileDialog.GetSelected().string() << std::endl;
							fileDialog.ClearSelected();
						}
					}
					ImGui::EndMenu();
				}
				ImGui::EndMenu();
			}
			ImGui::MenuItem("Open Scene", "Ctrl+O");
			ImGui::MenuItem("Save Scene", "Ctrl+S");
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Edit"))
		{
			ImGui::MenuItem("Undo", "Ctrl+Z");
			ImGui::MenuItem("Redo", "Ctrl+Y");
			ImGui::Separator();
			ImGui::MenuItem("Cut", "Ctrl+X");
			ImGui::MenuItem("Copy", "Ctrl+C");
			ImGui::MenuItem("Paste", "Ctrl+V");
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("View"))
		{
			ImGui::MenuItem("Toggle Statistics Window", nullptr, nullptr, true);
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}
}
