#pragma once
#include "imgui.h"
#include "imfilebrowser.h"
#include "ImGuizmo.h"
#include "imterm/terminal_helpers.hpp"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/sink.h"
#include <functional>
#include <vector>
#include <utility>
#include <iostream>
#include <algorithm>
#include <memory>
#include <mutex>

#include "../Scene/Scene.h"
#include "../Graphics/Camera.h"
#include "../Graphics/Graphics.h"
#include "../Graphics/Lighting/DirectionalLight.h"
#include "../Graphics/Lighting/PointLight.h"
#include "../Graphics/Lighting/SpotLight.h"
#include "../Input/Mouse.h"

class ImguiManager
{
public:
	struct LogTerminalHelper : ImTerm::basic_spdlog_terminal_helper<LogTerminalHelper, void, std::mutex>
	{
		using basic_spdlog_terminal_helper::basic_spdlog_terminal_helper;
	};
	using LogTerminal = ImTerm::terminal<LogTerminalHelper>;

	struct UiContext
	{
		Scene* scene = nullptr;
		Camera* activeCamera = nullptr;
		Graphics* graphics = nullptr;
		std::vector<PointLight*>* pointLights = nullptr;
		std::vector<SpotLight*>* spotLights = nullptr;
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
	// cache size of window for dynamic resizing
	int width;
	int height;
	ImGuizmo::OPERATION currentOperation = ImGuizmo::TRANSLATE;
	ImGuizmo::MODE currentMode = ImGuizmo::WORLD;

	std::shared_ptr<LogTerminalHelper> logTerminalHelper;
	std::shared_ptr<spdlog::sinks::sink> logTerminalSink;
	std::unique_ptr<LogTerminal> logTerminal;
};
