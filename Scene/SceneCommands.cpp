#include "../App.h"
#include "../Serialization/SceneFormat.h"
#include "../Serialization/AssetPath.h"
#include <sstream>
#include <algorithm>
#include <cctype>

namespace
{
bool IsScenePath(const std::filesystem::path& path)
{
	auto extension = path.extension().string();
	std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return extension == SceneFormat::Extension;
}
}

void App::UpdateUiContext()
{
	ImguiManager::UiContext context;
	context.scene = &scene; context.activeCamera = activeCam; context.graphics = &wnd.Gfx();
	context.pointLights = &pointLights; context.spotLights = &spotLights; context.directionalLights = &directionalLights;
	context.mouse = &wnd.mouse; context.isPlayMode = &isPlayMode; context.isPaused = &isPaused;
	context.resetSimulation = [this] { RequestSceneCommand(SceneCommand::Stop); };
	context.physicsDebugSettings = &physics.GetDebugDrawSettings();
	context.sceneEditor = &sceneEditor;
	context.sceneCommand = [this](SceneCommand command, const std::filesystem::path& path) { RequestSceneCommand(command, path); };
	imgui.SetContext(std::move(context));
}

void App::RequestSceneCommand(SceneCommand command, const std::filesystem::path& path)
{
	if (pendingSceneCommand != SceneCommand::None) return;
	const bool ordinary = command == SceneCommand::Open || command == SceneCommand::Save || command == SceneCommand::SaveAs || command == SceneCommand::Play;
	if (ordinary && (isPlayMode || sceneEditor.dialog != SceneDialog::None)) return;
	if ((command == SceneCommand::Stop || command == SceneCommand::TogglePause) && !isPlayMode) return;
	if (command == SceneCommand::ChoosePath && sceneEditor.dialog != SceneDialog::Open && sceneEditor.dialog != SceneDialog::SaveAs) return;
	if ((command == SceneCommand::SaveChanges || command == SceneCommand::DiscardChanges) && sceneEditor.dialog != SceneDialog::UnsavedChanges) return;
	if (command == SceneCommand::ConfirmOverwrite && sceneEditor.dialog != SceneDialog::Overwrite) return;
	if (command == SceneCommand::DismissError && sceneEditor.dialog != SceneDialog::Error) return;
	pendingSceneCommand = command; pendingCommandPath = path;
}

void App::ShowSceneDialog(SceneDialog dialog, const std::filesystem::path& path, std::string message)
{
	sceneEditor.dialog = dialog; sceneEditor.dialogPath = path; sceneEditor.message = std::move(message);
	++sceneEditor.dialogRevision;
}

void App::ReportSceneResult(const LoadResult& result, const std::string& operation)
{
	std::ostringstream messages;
	for (const auto& warning : result.warnings)
		TE_LOGWARNING("{}: {}: {} (object '{}', component '{}')", operation, warning.jsonPath, warning.message, warning.objectName, warning.componentType);
	for (const auto& error : result.errors)
	{
		TE_LOGERROR("{}: {}: {} (object '{}', component '{}')", operation, error.jsonPath, error.message, error.objectName, error.componentType);
		messages << operation << ": " << error.jsonPath << ": " << error.message;
		if (!error.objectName.empty()) messages << "\nObject: " << error.objectName;
		if (error.objectId) messages << " (" << error.objectId->ToString() << ")";
		if (!error.componentType.empty()) messages << "\nComponent: " << error.componentType;
		if (error.componentId) messages << " (" << error.componentId->ToString() << ")";
		messages << "\n";
	}
	if (!result) ShowSceneDialog(SceneDialog::Error, {}, messages.str());
}

bool App::HasUnsavedSceneChanges() const
{
	if (sceneEditor.currentPath.empty() || recoveryUnsaved) return true;
	try { return SceneSerializer::Serialize(scene, { projectRoot }) != savedSceneDocument; }
	catch (const std::exception&) { return true; }
}

bool App::SaveSceneFile(const std::filesystem::path& path)
{
	try
	{
		const auto absolute = std::filesystem::absolute(path).lexically_normal();
		const auto document = SceneSerializer::Serialize(scene, { projectRoot });
		const auto result = SceneSerializer::SaveToFile(scene, absolute, { projectRoot });
		ReportSceneResult(result, "Save " + AssetPath::ToUtf8(absolute));
		if (!result) { afterSceneSave = AfterSceneSave::None; pendingOpenPath.clear(); return false; }
		sceneEditor.currentPath = absolute; savedSceneDocument = document; recoveryUnsaved = false;
		ShowSceneDialog(SceneDialog::None);
		ResetFrameTiming(); sceneTimingReset = true;
		TE_LOG("Saved scene to {}", AssetPath::ToUtf8(absolute));
		CompleteSceneSave(); return true;
	}
	catch (const std::exception& error)
	{
		LoadResult result;
		if (const auto* serialization = dynamic_cast<const SerializationException*>(&error)) result.errors.push_back(serialization->diagnostic);
		else { SerializationError diagnostic; diagnostic.message = error.what(); result.errors.push_back(std::move(diagnostic)); }
		ReportSceneResult(result, "Save scene"); afterSceneSave = AfterSceneSave::None; pendingOpenPath.clear(); return false;
	}
}

void App::CompleteSceneSave()
{
	const auto continuation = afterSceneSave; afterSceneSave = AfterSceneSave::None;
	if (continuation == AfterSceneSave::Play)
	{
		prePlayDocument = savedSceneDocument; prePlayPath = sceneEditor.currentPath;
		prePlayGravity = physics.GetGravity();
		isPlayMode = true; isPaused = false;
		activeCam = gameCams.empty() ? &editorCam : gameCams.front();
	}
	else if (continuation == AfterSceneSave::Open) OpenSceneFile(pendingOpenPath);
}

void App::FinishSceneReplacement()
{
	persistentObjects.clear(); physics.ClearDebugDrawCache(); physicsDebugVertices.clear();
	CacheSceneComponents(); activeCam = &editorCam;
	ResetFrameTiming(); sceneTimingReset = true;
	UpdateUiContext();
}

bool App::OpenSceneFile(const std::filesystem::path& path)
{
	const auto absolute = std::filesystem::absolute(path).lexically_normal();
	LoadContext context{ wnd.Gfx(), projectRoot };
	const auto result = SceneSerializer::LoadFromFile(absolute, scene, context);
	ReportSceneResult(result, "Open " + AssetPath::ToUtf8(absolute));
	pendingOpenPath.clear(); afterSceneSave = AfterSceneSave::None;
	if (!result) return false;
	audio.StopAll(); audio.Suspend();
	sceneEditor.currentPath = absolute; savedSceneDocument = SceneSerializer::Serialize(scene, { projectRoot }); recoveryUnsaved = false;
	ShowSceneDialog(SceneDialog::None); FinishSceneReplacement();
	TE_LOG("Opened scene from {}", AssetPath::ToUtf8(absolute)); return true;
}

void App::ResetSimulation()
{
	isPlayMode = false; isPaused = false;
	audio.Suspend(); audio.StopAll();
	LoadContext context{ wnd.Gfx(), projectRoot };
	const auto result = SceneSerializer::LoadFromFile(prePlayPath, scene, context);
	ReportSceneResult(result, "Restore " + AssetPath::ToUtf8(prePlayPath));
	bool restored = static_cast<bool>(result);
	if (!restored && !prePlayDocument.is_null())
	{
		const auto fileError = sceneEditor.message;
		const auto backup = SceneSerializer::Deserialize(prePlayDocument, scene, context);
		ReportSceneResult(backup, "Restore pre-play backup");
		restored = static_cast<bool>(backup);
		ShowSceneDialog(SceneDialog::Error, {}, fileError + (restored ? "\nRestored the pre-play backup. Save Scene to repair the scene file." : "\n" + sceneEditor.message + "\nThe current scene was retained in Edit mode."));
	}
	if (restored)
	{
		physics.SetGravity(prePlayGravity);
		savedSceneDocument = SceneSerializer::Serialize(scene, { projectRoot });
		recoveryUnsaved = !result;
	}
	else recoveryUnsaved = true;
	FinishSceneReplacement();
}

void App::ProcessSceneCommands()
{
	const auto command = pendingSceneCommand;
	const auto path = pendingCommandPath;
	pendingSceneCommand = SceneCommand::None; pendingCommandPath.clear();
	auto save = [&] {
		if (sceneEditor.currentPath.empty()) ShowSceneDialog(SceneDialog::SaveAs, projectRoot / "Scene.scene");
		else SaveSceneFile(sceneEditor.currentPath);
	};
	try
	{
	switch (command)
	{
	case SceneCommand::Open: ShowSceneDialog(SceneDialog::Open, sceneEditor.currentPath.empty() ? projectRoot : sceneEditor.currentPath); break;
	case SceneCommand::Save: afterSceneSave = AfterSceneSave::None; save(); break;
	case SceneCommand::SaveAs: afterSceneSave = AfterSceneSave::None; ShowSceneDialog(SceneDialog::SaveAs, sceneEditor.currentPath.empty() ? projectRoot / "Scene.scene" : sceneEditor.currentPath); break;
	case SceneCommand::Play: afterSceneSave = AfterSceneSave::Play; save(); break;
	case SceneCommand::Stop: ResetSimulation(); break;
	case SceneCommand::TogglePause: isPaused = !isPaused; break;
	case SceneCommand::ChoosePath:
		if (sceneEditor.dialog == SceneDialog::Open)
		{
			if (!IsScenePath(path)) { ShowSceneDialog(SceneDialog::Error, {}, "Choose a .scene file."); break; }
			pendingOpenPath = path;
			if (HasUnsavedSceneChanges()) ShowSceneDialog(SceneDialog::UnsavedChanges);
			else OpenSceneFile(path);
		}
		else
		{
			pendingSavePath = path; pendingSavePath.replace_extension(".scene");
			if (std::filesystem::exists(pendingSavePath)) ShowSceneDialog(SceneDialog::Overwrite, pendingSavePath);
			else SaveSceneFile(pendingSavePath);
		}
		break;
	case SceneCommand::SaveChanges: afterSceneSave = AfterSceneSave::Open; save(); break;
	case SceneCommand::DiscardChanges: OpenSceneFile(pendingOpenPath); break;
	case SceneCommand::ConfirmOverwrite: SaveSceneFile(pendingSavePath); break;
	case SceneCommand::Cancel:
	case SceneCommand::DismissError:
		afterSceneSave = AfterSceneSave::None; pendingOpenPath.clear(); pendingSavePath.clear(); ShowSceneDialog(SceneDialog::None); break;
	default: break;
	}
	}
	catch (const std::exception& error)
	{
		afterSceneSave = AfterSceneSave::None; pendingOpenPath.clear(); pendingSavePath.clear();
		LoadResult result; SerializationError diagnostic; diagnostic.message = error.what(); result.errors.push_back(std::move(diagnostic));
		ReportSceneResult(result, "Scene operation");
	}
}
