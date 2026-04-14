#include "ImguiManager.h"

ImguiManager::ImguiManager()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	fileDialog.SetTitle("Select Model File");

	logTerminalHelper = std::make_shared<LogTerminalHelper>();
	logTerminalSink = logTerminalHelper;

	int logSizeX = static_cast<int>(width / 1920.0f * 1280.0f);
	int logSizeY = static_cast<int>(height / 1080.0f * 270.0f);
	logTerminal = std::make_unique<LogTerminal>("##EngineLogTerminal", logSizeX, logSizeY, logTerminalHelper);
	logTerminal->set_flags(
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoCollapse
	);
	if (const auto logger = spdlog::default_logger())
	{
		logger->sinks().push_back(logTerminalSink);
	}
	TE_LOG("ImguiManager initialized and log terminal sink added to spdlog default logger.");
}

ImguiManager::~ImguiManager()
{
	if (const auto logger = spdlog::default_logger())
	{
		auto& sinks = logger->sinks();
		sinks.erase(std::remove(sinks.begin(), sinks.end(), logTerminalSink), sinks.end());
	}
	ImGui::DestroyContext();
}

void ImguiManager::SetContext(UiContext context) noexcept
{
	this->context = std::move(context);
	// Cache window size for dynamic resizing
	width = this->context.graphics->GetWidth();
	height = this->context.graphics->GetHeight();
}

void ImguiManager::DrawGizmo() noexcept
{
	if (context.scene == nullptr || context.activeCamera == nullptr || context.graphics == nullptr)
	{
		return;
	}
	if (context.isPlayMode != nullptr && *context.isPlayMode)
	{
		return;
	}
	if (context.scene->GetSelectedObject() == nullptr)
	{
		return;
	}

	// Relative size and position based on cached size of window
	int PosX = static_cast<int>(width / 1920.0f * 300.0f);
	int PosY = static_cast<int>(height / 1080.0f * 60.0f);
	int SizeX = static_cast<int>(width / 1920.0f * 1280.0f);
	int SizeY = static_cast<int>(height / 1080.0f * 720.0f);
	ImGui::SetNextWindowPos(ImVec2(PosX, PosY), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(SizeX, SizeY), ImGuiCond_Always);
	ImGui::Begin("Viewport", nullptr,
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoScrollWithMouse |
		ImGuiWindowFlags_NoBackground |
		ImGuiWindowFlags_NoMouseInputs
	);

	const bool isViewportActive =
		ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) ||
		ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
	if (isViewportActive)
	{
		if (ImGui::IsKeyPressed(ImGuiKey_T)) currentOperation = ImGuizmo::TRANSLATE;
		if (ImGui::IsKeyPressed(ImGuiKey_R)) currentOperation = ImGuizmo::ROTATE;
		if (ImGui::IsKeyPressed(ImGuiKey_S)) currentOperation = ImGuizmo::SCALE;
	}

	ImGui::SetCursorPos(ImVec2(10.0f, 10.0f));
	float GizmoToolbarWidth = width / 1920.0f * 240.0f;
	float GizmoToolbarHeight = height / 1080.0f * 90.0f;
	ImGui::BeginChild("##gizmo_toolbar", ImVec2(GizmoToolbarWidth, GizmoToolbarHeight), true);
	if (ImGui::RadioButton("Translate", currentOperation == ImGuizmo::TRANSLATE)) currentOperation = ImGuizmo::TRANSLATE;
	ImGui::SameLine();
	if (ImGui::RadioButton("Rotate", currentOperation == ImGuizmo::ROTATE)) currentOperation = ImGuizmo::ROTATE;
	ImGui::SameLine();
	if (ImGui::RadioButton("Scale", currentOperation == ImGuizmo::SCALE)) currentOperation = ImGuizmo::SCALE;
	if (currentOperation != ImGuizmo::SCALE)
	{
		if (ImGui::RadioButton("Local", currentMode == ImGuizmo::LOCAL)) currentMode = ImGuizmo::LOCAL;
		ImGui::SameLine();
		if (ImGui::RadioButton("World", currentMode == ImGuizmo::WORLD)) currentMode = ImGuizmo::WORLD;
	}
	ImGui::EndChild();

	ImGuizmo::SetDrawlist();
	const auto viewportPos = ImGui::GetWindowPos();
	const auto viewportSize = ImGui::GetWindowSize();
	ImGuizmo::SetRect(viewportPos.x, viewportPos.y, viewportSize.x, viewportSize.y);

	DirectX::XMFLOAT4X4 gameObjectWorldMatrix;
	DirectX::XMFLOAT4X4 viewMatrix;
	DirectX::XMFLOAT4X4 projectionMatrix;
	DirectX::XMStoreFloat4x4(&gameObjectWorldMatrix, context.scene->GetSelectedWorldTransformMatrix());
	DirectX::XMStoreFloat4x4(&viewMatrix, context.activeCamera->GetViewMatrix());
	DirectX::XMStoreFloat4x4(&projectionMatrix, context.graphics->GetProjection());

	if (ImGuizmo::Manipulate(
		&viewMatrix.m[0][0],
		&projectionMatrix.m[0][0],
		currentOperation,
		currentMode,
		&gameObjectWorldMatrix.m[0][0]
	))
	{
		context.scene->SetSelectedWorldTransformMatrix(DirectX::XMLoadFloat4x4(&gameObjectWorldMatrix));
	}

	ImGui::End();
}

void ImguiManager::EditorWindow(bool* p_open)
{
	int SizeX = static_cast<int>(width / 1920.0f * 300.0f);
	int SizeY = static_cast<int>(height / 1080.0f * 60.0f);
	ImGui::SetNextWindowSize(ImVec2(SizeX, SizeY), ImGuiCond_Always);
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
	int TitleSizeX = static_cast<int>(width / 1920.0f * 1280.0f);
	int TitleSizeY = static_cast<int>(height / 1080.0f * 60.0f);
	int TitlePosX = static_cast<int>(width / 1920.0f * 300.0f);
	ImGui::SetNextWindowSize(ImVec2(TitleSizeX, TitleSizeY), ImGuiCond_Always);
	ImGui::SetNextWindowPos(ImVec2(TitlePosX, 0), ImGuiCond_Always);
	// TODO: version number should be automatically updated during build process
	if (ImGui::Begin("TapiEngine v0.6", nullptr,
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

	MultipurposeWindow();
	SettingsWindow();

	if (context.scene != nullptr)
	{
		context.scene->DrawHierarchyWindow();
		context.scene->DrawInspectorWindow();
	}

	DrawGizmo();

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
	//if (context.spotLights != nullptr)
	//{
	//	for (auto* light : *context.spotLights)
	//	{
	//		if (light != nullptr)
	//		{
	//			light->SpawnControlWindow();
	//		}
	//	}
	//}
	//if (context.directionalLights != nullptr)
	//{
	//	for (auto* light : *context.directionalLights)
	//	{
	//		if (light != nullptr)
	//		{
	//			light->SpawnControlWindow();
	//		}
	//	}
	//}
	MainMenuBar();
}


inline void ImguiManager::SettingsWindow()
{
	if (!settingsWindowOpen)
	{
		return;
	}

	ImGui::Begin("Settings", &settingsWindowOpen);
	if (context.graphics != nullptr)
	{
		auto& wireframeSettings = context.graphics->GetWireframeDebugSettings();
		ImGui::Checkbox("Draw BVH Wireframes", &wireframeSettings.enabled);
		ImGui::ColorEdit3("Wireframe Color", &wireframeSettings.color.x);
		ImGui::Separator();
	}
	if (context.mouse != nullptr)
	{
		bool rawEnabled = context.mouse->RawEnabled();
		if (ImGui::Checkbox("Enable Raw Mouse Input", &rawEnabled))
		{
			if (rawEnabled)
			{
				context.mouse->EnableRaw();
			}
			else
			{
				context.mouse->DisableRaw();
			}
		}
	}
	ImGui::End();
}

inline void ImguiManager::MultipurposeWindow()
{
	int PosX = static_cast<int>(width / 1920.0f * 300.0f);
	int PosY = static_cast<int>(height / 1080.0f * 780.0f);
	int SizeX = static_cast<int>(width / 1920.0f * 1280.0f);
	int SizeY = static_cast<int>(height / 1080.0f * 300.0f);
	ImGui::SetNextWindowSize(ImVec2(SizeX, SizeY), ImGuiCond_Always);
	ImGui::SetNextWindowPos(ImVec2(PosX, PosY), ImGuiCond_Always);
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
			//ImGui::TextUnformatted("This is a log window. Redirect application's log output here.");
			if (logTerminal != nullptr)
			{
				int logPosX = static_cast<int>(width / 1920.0f * 300.0f);
				int logPosY = static_cast<int>(height / 1080.0f * 810.0f);
				ImGui::SetNextWindowPos(ImVec2(logPosX, logPosY), ImGuiCond_Always);
				logTerminal->show();
			}
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

}

inline void ImguiManager::MainMenuBar()
{
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
			ImGui::MenuItem("Open Settings", nullptr, &settingsWindowOpen);
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
