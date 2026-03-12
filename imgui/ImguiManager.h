#pragma once
#include "imgui.h"
#include "imfilebrowser.h"
#include <iostream>

class ImguiManager
{
public:
	ImguiManager();
	~ImguiManager();

public:
	void StatWindow(bool* p_open = nullptr);
private:
	ImGui::FileBrowser fileDialog;
};