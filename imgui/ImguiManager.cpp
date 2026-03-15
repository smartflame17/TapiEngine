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

	ImGui::SetNextWindowPos(ImVec2(300, 60), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(1280, 720), ImGuiCond_Always);
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

	if (ImGui::IsWindowHovered())
	{
		if (ImGui::IsKeyPressed(ImGuiKey_T)) currentOperation = ImGuizmo::TRANSLATE;
		if (ImGui::IsKeyPressed(ImGuiKey_R)) currentOperation = ImGuizmo::ROTATE;
		if (ImGui::IsKeyPressed(ImGuiKey_S)) currentOperation = ImGuizmo::SCALE;
	}

	ImGui::SetCursorPos(ImVec2(10.0f, 10.0f));
	ImGui::BeginChild("##gizmo_toolbar", ImVec2(240.0f, 90.0f), true);
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

	DirectX::XMFLOAT4X4 modelMatrix;
	DirectX::XMFLOAT4X4 viewMatrix;
	DirectX::XMFLOAT4X4 projectionMatrix;
	DirectX::XMStoreFloat4x4(&modelMatrix, context.scene->GetSelectedWorldTransformMatrix());
	DirectX::XMStoreFloat4x4(&viewMatrix, context.activeCamera->GetViewMatrix());
	DirectX::XMStoreFloat4x4(&projectionMatrix, context.graphics->GetProjection());

	if (ImGuizmo::Manipulate(
		&viewMatrix.m[0][0],
		&projectionMatrix.m[0][0],
		currentOperation,
		currentMode,
		&modelMatrix.m[0][0]
	))
	{
		context.scene->SetSelectedWorldTransformMatrix(DirectX::XMLoadFloat4x4(&modelMatrix));
	}

	ImGui::End();
}

void ImguiManager::EditorWindow(bool* p_open)
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
