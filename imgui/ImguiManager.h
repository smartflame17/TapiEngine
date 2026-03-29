#pragma once
#include "imgui.h"
#include "imfilebrowser.h"
#include "ImGuizmo.h"
#include <functional>
#include <vector>
#include <utility>
#include <iostream>

#include "../Scene/Scene.h"
#include "../Graphics/Camera.h"
#include "../Graphics/Graphics.h"
#include "../Graphics/Lighting/DirectionalLight.h"
#include "../Graphics/Lighting/PointLight.h"
#include "../Input/Mouse.h"

class ImguiManager
{
public:
	struct UiContext
	{
		Scene* scene = nullptr;
		Camera* activeCamera = nullptr;
		Graphics* graphics = nullptr;
		std::vector<PointLight*>* pointLights = nullptr;
		std::vector<DirectionalLight*>* directionalLights = nullptr;
		Mouse* mouse = nullptr;
		bool* isPlayMode = nullptr;
		bool* isPaused = nullptr;
		std::function<void()> resetSimulation;
	};

public:
	ImguiManager();
	~ImguiManager();

	void SetContext(UiContext context) noexcept;
	void EditorWindow(bool* p_open = nullptr);
	inline void MainMenuBar();
	inline void MultipurposeWindow();
	inline void SettingsWindow();

private:
	void DrawGizmo() noexcept;

private:
	UiContext context;
	ImGui::FileBrowser fileDialog;
	bool settingsWindowOpen = false;
	ImGuizmo::OPERATION currentOperation = ImGuizmo::TRANSLATE;
	ImGuizmo::MODE currentMode = ImGuizmo::WORLD;
};
