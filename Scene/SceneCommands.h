#pragma once
#include <filesystem>
#include <string>
#include <cstdint>

enum class SceneCommand
{
	None, Open, Save, SaveAs, Play, Stop, TogglePause,
	ChoosePath, SaveChanges, DiscardChanges, ConfirmOverwrite, Cancel, DismissError
};
enum class SceneDialog { None, Open, SaveAs, UnsavedChanges, Overwrite, Error };
struct SceneEditorState
{
	SceneDialog dialog = SceneDialog::None;
	std::uint64_t dialogRevision = 0;
	std::filesystem::path currentPath;
	std::filesystem::path dialogPath;
	std::string message;
};
