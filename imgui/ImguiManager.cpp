#include "ImguiManager.h"
#include "../Physics/PhysicsDebugDraw.h"

ImguiManager::ImguiManager()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	fileDialog.SetTitle("Select Model File");
	openSceneDialog.SetTitle("Open Scene"); openSceneDialog.SetTypeFilters({ ".scene" });
	saveSceneDialog.SetTitle("Save Scene As"); saveSceneDialog.SetTypeFilters({ ".scene" });

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
	logTerminalHelper->set_formatter(
		std::make_unique<spdlog::pattern_formatter>("%^[%T.%e] [%l] %v%$")
	);

	auto& levelColors = logTerminal->theme().log_level_colors;
	const ImTerm::theme::constexpr_color white{ 1.0f, 1.0f, 1.0f, 1.0f };
	const ImTerm::theme::constexpr_color yellow{ 1.0f, 0.85f, 0.20f, 1.0f };
	const ImTerm::theme::constexpr_color red{ 1.0f, 0.25f, 0.25f, 1.0f };
	const ImTerm::theme::constexpr_color green{ 0.25f, 1.0f, 0.25f, 1.0f };

	levelColors[ImTerm::message::severity::trace] = white;
	levelColors[ImTerm::message::severity::debug] = white;
	levelColors[ImTerm::message::severity::info] = green;

	levelColors[ImTerm::message::severity::warn] = yellow;
	levelColors[ImTerm::message::severity::err] = red;
	levelColors[ImTerm::message::severity::critical] = red;
	if (const auto logger = spdlog::default_logger())
	{
		logger->sinks().push_back(logTerminalSink);
	}
	TE_LOG("ImguiManager initialized and log terminal sink added to spdlog default logger.");
	TE_LOGERROR("This is an error message to test the log terminal sink.");
	TE_LOGWARNING("This is a warning message to test the log terminal sink.");
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
	if ((context.sceneEditor && context.sceneEditor->dialog != SceneDialog::None) || ImGui::GetIO().KeyCtrl) return;
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
	
	char buffer[256];
	GetPrivateProfileStringA("Settings", "Version", "TapiEngine v??", buffer, sizeof(buffer), ".\\config.ini");
	if (ImGui::Begin(buffer, nullptr,
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
				if (context.sceneCommand) context.sceneCommand(*context.isPlayMode ? SceneCommand::TogglePause : SceneCommand::Play, {});
			}

			ImGui::SameLine();
			ImGui::BeginDisabled(!*context.isPlayMode);
			if (ImGui::Button(stopBtnLabel))
			{
				if (*context.isPlayMode)
				{
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
	DrawSceneDialogs();
}


inline void ImguiManager::SettingsWindow()
{
	if (!settingsWindowOpen)
	{
		return;
	}

	ImGui::SetNextWindowSize(ImVec2(560, 220), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowPos(ImVec2(width * 0.5f, height * 0.5f), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
	ImGui::Begin("Settings", &settingsWindowOpen);
	if (ImGui::BeginTabBar("SettingsTabs"))
	{
		if (ImGui::BeginTabItem("General"))
		{
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
					if (rawEnabled) context.mouse->EnableRaw();
					else context.mouse->DisableRaw();
				}
			}
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Physics"))
		{
			if (context.physicsDebugSettings != nullptr)
			{
				auto& settings = *context.physicsDebugSettings;
				ImGui::Checkbox("Draw Colliders During Play", &settings.drawDuringPlay);
				ImGui::ColorEdit3("Idle Collider Color", &settings.idleColor.x);
				ImGui::ColorEdit3("Colliding Collider Color", &settings.collisionColor.x);
			}
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
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
	const bool canUseFiles = context.sceneCommand && context.isPlayMode && !*context.isPlayMode &&
		context.sceneEditor && context.sceneEditor->dialog == SceneDialog::None;
	if (canUseFiles)
	{
		if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_O, ImGuiInputFlags_RouteGlobal)) context.sceneCommand(SceneCommand::Open, {});
		if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S, ImGuiInputFlags_RouteGlobal)) context.sceneCommand(SceneCommand::Save, {});
		if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_S, ImGuiInputFlags_RouteGlobal)) context.sceneCommand(SceneCommand::SaveAs, {});
	}
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
			if (ImGui::MenuItem("Open Scene", "Ctrl+O", false, canUseFiles)) context.sceneCommand(SceneCommand::Open, {});
			if (ImGui::MenuItem("Save Scene", "Ctrl+S", false, canUseFiles)) context.sceneCommand(SceneCommand::Save, {});
			if (ImGui::MenuItem("Save Scene As", "Ctrl+Shift+S", false, canUseFiles)) context.sceneCommand(SceneCommand::SaveAs, {});
			if (context.sceneEditor && !context.sceneEditor->currentPath.empty())
				ImGui::TextUnformatted(reinterpret_cast<const char*>(context.sceneEditor->currentPath.filename().u8string().c_str()));
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

void ImguiManager::DrawSceneDialogs()
{
	if (!context.sceneEditor || !context.sceneCommand) return;
	const auto& state = *context.sceneEditor;
	const char* popup = state.dialog == SceneDialog::UnsavedChanges ? "Unsaved Scene Changes" :
		state.dialog == SceneDialog::Overwrite ? "Overwrite Scene" : state.dialog == SceneDialog::Error ? "Scene Operation Failed" : nullptr;
	if (sceneDialogRevision != state.dialogRevision)
	{
		sceneDialogRevision = state.dialogRevision;
		openSceneDialog.Close(); saveSceneDialog.Close();
		if (state.dialog == SceneDialog::Open || state.dialog == SceneDialog::SaveAs)
		{
			auto& browser = state.dialog == SceneDialog::Open ? openSceneDialog : saveSceneDialog;
			const auto directory = std::filesystem::is_directory(state.dialogPath) ? state.dialogPath : state.dialogPath.parent_path();
			browser.SetDirectory(directory);
			if (state.dialog == SceneDialog::SaveAs)
			{
				const auto filename = state.dialogPath.filename().u8string();
				browser.SetInputName(std::string(reinterpret_cast<const char*>(filename.data()), filename.size()));
			}
			browser.Open();
		}
		else if (popup) ImGui::OpenPopup(popup);
	}
	if (state.dialog == SceneDialog::Open || state.dialog == SceneDialog::SaveAs)
	{
		auto& browser = state.dialog == SceneDialog::Open ? openSceneDialog : saveSceneDialog;
		browser.Display();
		if (browser.HasSelected())
		{
			const auto path = browser.GetSelected(); browser.ClearSelected();
			context.sceneCommand(SceneCommand::ChoosePath, path);
		}
		else if (!browser.IsOpened()) context.sceneCommand(SceneCommand::Cancel, {});
	}
	if (popup && ImGui::BeginPopupModal(popup, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		if (state.dialog == SceneDialog::UnsavedChanges)
		{
			ImGui::TextUnformatted("Save changes before opening another scene?");
			if (ImGui::Button("Save")) { context.sceneCommand(SceneCommand::SaveChanges, {}); ImGui::CloseCurrentPopup(); }
			ImGui::SameLine();
			if (ImGui::Button("Discard")) { context.sceneCommand(SceneCommand::DiscardChanges, {}); ImGui::CloseCurrentPopup(); }
			ImGui::SameLine();
			if (ImGui::Button("Cancel")) { context.sceneCommand(SceneCommand::Cancel, {}); ImGui::CloseCurrentPopup(); }
		}
		else if (state.dialog == SceneDialog::Overwrite)
		{
			ImGui::TextUnformatted("Replace the existing scene file?");
			ImGui::TextUnformatted(reinterpret_cast<const char*>(state.dialogPath.u8string().c_str()));
			if (ImGui::Button("Overwrite")) { context.sceneCommand(SceneCommand::ConfirmOverwrite, {}); ImGui::CloseCurrentPopup(); }
			ImGui::SameLine();
			if (ImGui::Button("Cancel")) { context.sceneCommand(SceneCommand::Cancel, {}); ImGui::CloseCurrentPopup(); }
		}
		else
		{
			ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 600);
			ImGui::TextUnformatted(state.message.c_str()); ImGui::PopTextWrapPos();
			if (ImGui::Button("OK")) { context.sceneCommand(SceneCommand::DismissError, {}); ImGui::CloseCurrentPopup(); }
		}
		ImGui::EndPopup();
	}
}
