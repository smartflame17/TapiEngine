#pragma once
#include "imgui.h"
#include <functional>
#include <vector>
#include <utility>

#include "../Scene/Scene.h"
#include "../Graphics/Camera.h"
#include "../Graphics/Lighting/PointLight.h"

class ImguiManager
{
public:
	struct UiContext
	{
		Scene* scene = nullptr;
		Camera* activeCamera = nullptr;
		std::vector<PointLight*>* pointLights = nullptr;
		bool* isPlayMode = nullptr;
		bool* isPaused = nullptr;
		std::function<void()> resetSimulation;
	};

public:
	ImguiManager();
	~ImguiManager();

	void SetContext(UiContext context) noexcept;
	void StatWindow(bool* p_open = nullptr);

private:
	UiContext context;
};
