#include "../App.h"
#include "SerializationScriptFixtures.h"
#include "PhysicsTestAccess.h"
#include "PhysicsComponentTestAccess.h"
#include <box3d/box3d.h>
#include <fstream>
#include <iostream>
#include <cwctype>

class AppSceneTestAccess
{
public:
	static void Command(App& app, SceneCommand command, const std::filesystem::path& path = {}) { app.RequestSceneCommand(command, path); app.Update(0); }
	static void Frame(App& app, float delta) { app.Update(delta); }
	static void Render(App& app) { app.RenderFrame(0); }
	static Scene& SceneOf(App& app) { return app.scene; }
	static const SceneEditorState& Editor(const App& app) { return app.sceneEditor; }
	static bool Editing(const App& app) { return !app.isPlayMode && !app.isPaused; }
	static bool Playing(const App& app) { return app.isPlayMode; }
	static bool Dirty(const App& app) { return app.HasUnsavedSceneChanges(); }
	static bool EditorCamera(const App& app) { return app.activeCam == &app.editorCam; }
	static bool Paused(const App& app) { return app.isPaused; }
	static std::size_t Cameras(const App& app) { return app.gameCams.size(); }
	static std::size_t Points(const App& app) { return app.pointLights.size(); }
	static std::size_t Spots(const App& app) { return app.spotLights.size(); }
	static std::size_t Directions(const App& app) { return app.directionalLights.size(); }
	static void BreakBackup(App& app) { app.prePlayDocument["version"] = 999; }
	static HWND WindowOf(App& app) { return FindWindowA(nullptr, app.config.title.c_str()); }
	static void ReleaseCursor(App& app) { app.wnd.EnableCursor(); }
	static void Escape(App& app)
	{
		ImGui::GetIO().WantCaptureKeyboard = false;
		SendMessageA(WindowOf(app), WM_KEYDOWN, VK_ESCAPE, 0); app.HandleInput(app.dt);
		SendMessageA(WindowOf(app), WM_KEYUP, VK_ESCAPE, 0); app.Update(10);
	}
	static double Alpha(const App& app) { return app.fixedClock.GetAlpha(); }
};
namespace
{
int checks = 0;
void Check(bool value, const char* message) { ++checks; if (!value) throw std::runtime_error(message); }
nlohmann::json Read(const std::filesystem::path& path) { std::ifstream file(path); return nlohmann::json::parse(file); }
GameObject& Find(Scene& scene, const Guid& id)
{
	for (const auto& root : scene.GetRootObjects()) if (root->GetId() == id) return *root;
	throw std::runtime_error("Saved root UUID was not restored");
}
void TestApp(App& app, const std::filesystem::path& out)
{
	using Access = AppSceneTestAccess;
	auto command = [&](SceneCommand cmd, const std::filesystem::path& path = {}) { Access::Command(app, cmd, path); };
	auto& scene = Access::SceneOf(app);
	command(SceneCommand::Play);
	Check(Access::Editing(app) && Access::Editor(app).dialog == SceneDialog::SaveAs, "Untitled Play waits for Save As");
	command(SceneCommand::Cancel); Check(Access::Editing(app), "Canceling Save As cancels entering Play");
	auto& actor = scene.CreateGameObject("Authored actor"); actor.SetPosition(3, 8, 0);
	auto& script = actor.AddComponent<SerializationProbe>(); script.integer = 77; script.text = "before Play";
	auto& body = actor.AddComponent<Rigidbody>(); body.SetType(Rigidbody::Type::Dynamic); actor.AddComponent<Collider>();
	scene.CreateChildGameObject(actor, "nested camera").AddComponent<Camera>();
	const auto actorId = actor.GetId(); const auto scriptId = script.GetId();
	const auto oldBody = PhysicsComponentTestAccess::Body(body);
	const auto initialBodies = b3World_GetCounters(PhysicsTestAccess::World(Physics::GetInstance())).bodyCount;
	command(SceneCommand::SaveAs); command(SceneCommand::ChoosePath, out / "editor-workflow.txt");
	const auto path = out / "editor-workflow.scene";
	if (Access::Editor(app).dialog == SceneDialog::Overwrite) command(SceneCommand::ConfirmOverwrite);
	Check(Access::Editor(app).currentPath == path && std::filesystem::exists(path), "Save As normalizes the scene extension");
	Check(!Access::Dirty(app), "Successful save establishes the dirty baseline");
	script.integer = 101; actor.SetPosition(4, 9, 0);
	const auto authored = SceneSerializer::Serialize(scene);
	Physics::GetInstance().SetGravity({ 0, -3, 0 });
	SerializationProbe::ResetCounts(); command(SceneCommand::Play);
	Check(Access::Playing(app) && Read(path) == authored, "Play saves current edits to disk before simulation");
	Check(SerializationProbe::starts == 0, "Entering Play discards boundary time before script activation");
	Access::Frame(app, 1.0f / 60);
	Check(SerializationProbe::starts == 1 && SerializationProbe::updates == 1, "Scripts begin on the first running tick");
	command(SceneCommand::SaveAs); command(SceneCommand::Open); command(SceneCommand::Save);
	Check(Access::Editor(app).dialog == SceneDialog::None && Read(path) == authored, "File commands are disabled during Play");
	command(SceneCommand::TogglePause); command(SceneCommand::Open);
	Access::Frame(app, 1); Check(Access::Paused(app) && SerializationProbe::updates == 1, "Pause stops scripts and keeps file commands disabled");
	command(SceneCommand::TogglePause);
	script.integer = -999; actor.SetPosition(100, 100, 100);
	scene.CreateGameObject("runtime creation").AddComponent<Camera>();
	scene.GetRootObjects()[0]->Destroy(); scene.CleanupDestroyedObjects();
	Physics::GetInstance().SetGravity({0, -1, 0}); command(SceneCommand::Stop);
	Check(Access::Editing(app) && SceneSerializer::Serialize(scene) == authored, "Stop restores authored objects, fields, transforms and UUIDs");
	Check(Find(scene, actorId).GetComponent<SerializationProbe>()->GetId() == scriptId, "Script UUID survives simulation restoration");
	Check(!b3Body_IsValid(oldBody) && b3World_GetCounters(PhysicsTestAccess::World(Physics::GetInstance())).bodyCount == initialBodies,
		"Restoration replaces bodies and shapes without duplication");
	Check(Physics::GetInstance().GetGravity().y == -3 && Access::EditorCamera(app) && Access::Alpha(app) == 0,
		"Stop restores gravity, editor camera and frame timing");
	Check(Access::Cameras(app) == 2 && Access::Points(app) == 1 && Access::Spots(app) == 1 && Access::Directions(app) == 1,
		"Restoration refreshes nested camera and all light caches");
	Access::Render(app);
	for (int i = 0; i != 3; ++i)
	{
		command(SceneCommand::Open); command(SceneCommand::ChoosePath, path);
		Check(SceneSerializer::Serialize(scene) == authored && !Access::Dirty(app), "Repeated editor Open preserves the scene and clean baseline");
		Access::Render(app);
	}
	command(SceneCommand::Play); Find(scene, actorId).SetPosition(200, 0, 0); Access::Escape(app);
	Check(Access::Editing(app) && SceneSerializer::Serialize(scene) == authored && Access::Alpha(app) == 0, "Escape uses the same saved-file restore path");
	command(SceneCommand::Play);
	auto changedOnDisk = authored; changedOnDisk["scene"]["name"] = "Changed on disk";
	{ std::ofstream file(path); file << changedOnDisk.dump(); }
	command(SceneCommand::Stop);
	Check(scene.GetName() == "Changed on disk", "Stop loads the scene file rather than always using the in-memory backup");
	{ std::ofstream file(path); file << authored.dump(); }
	command(SceneCommand::Open); command(SceneCommand::ChoosePath, path);
	Scene other("Other scene"); const auto otherPath = out / "other.scene"; Check(static_cast<bool>(SceneSerializer::SaveToFile(other, otherPath)), "Second scene fixture is saved");
	Find(scene, actorId).SetName("unsaved actor"); const auto unsaved = SceneSerializer::Serialize(scene);
	command(SceneCommand::Open); command(SceneCommand::ChoosePath, otherPath);
	Check(Access::Editor(app).dialog == SceneDialog::UnsavedChanges, "Open prompts for unsaved changes");
	command(SceneCommand::Cancel); Check(SceneSerializer::Serialize(scene) == unsaved, "Canceling Open retains edits");
	command(SceneCommand::Open); command(SceneCommand::ChoosePath, otherPath); command(SceneCommand::SaveChanges);
	Check(Read(path) == unsaved && scene.GetName() == "Other scene" && Access::EditorCamera(app), "Save-before-Open saves the old scene and selects editor camera for camera-free scenes");
	command(SceneCommand::Open); command(SceneCommand::ChoosePath, path);
	Find(scene, actorId).SetName("discarded edit"); command(SceneCommand::Open); command(SceneCommand::ChoosePath, otherPath); command(SceneCommand::DiscardChanges);
	Check(scene.GetName() == "Other scene" && Read(path) == unsaved, "Discard opens the new scene without saving edits");
	command(SceneCommand::SaveAs); command(SceneCommand::ChoosePath, path);
	Check(Access::Editor(app).dialog == SceneDialog::Overwrite, "Save As confirms overwriting another scene");
	command(SceneCommand::Cancel); Check(Read(path) == unsaved, "Canceling overwrite preserves the file");
	command(SceneCommand::SaveAs); command(SceneCommand::ChoosePath, path); command(SceneCommand::ConfirmOverwrite);
	Check(Read(path)["scene"]["name"] == "Other scene", "Confirmed overwrite saves the current scene");
	const auto beforeFailure = SceneSerializer::Serialize(scene); const auto previousPath = Access::Editor(app).currentPath;
	command(SceneCommand::SaveAs); command(SceneCommand::ChoosePath, out / "absent-parent" / "failure.scene");
	Check(Access::Editor(app).dialog == SceneDialog::Error && Access::Editor(app).currentPath == previousPath && Read(path) == beforeFailure,
		"Failed Save As retains the previous path and saved file");
	command(SceneCommand::DismissError);
	const auto malformed = out / "malformed.scene"; { std::ofstream file(malformed); file << "{bad JSON"; }
	command(SceneCommand::Open); command(SceneCommand::ChoosePath, malformed);
	Check(Access::Editor(app).dialog == SceneDialog::Error && SceneSerializer::Serialize(scene) == beforeFailure && Access::Editor(app).currentPath == previousPath,
		"Invalid Open retains the active scene and path");
	command(SceneCommand::DismissError);
	auto& unsupported = scene.CreateGameObject("Unsupported audio"); unsupported.AddComponent<AudioClip>();
	command(SceneCommand::Open); command(SceneCommand::ChoosePath, otherPath); command(SceneCommand::SaveChanges);
	Check(Access::Editor(app).dialog == SceneDialog::Error && scene.GetRootObjects().size() == 1 && Read(path) == beforeFailure,
		"A failed save cancels a pending Open and retains unsaved objects");
	command(SceneCommand::DismissError);
	command(SceneCommand::Play);
	Check(Access::Editing(app) && Access::Editor(app).dialog == SceneDialog::Error && Read(path) == beforeFailure,
		"Unsupported components block Play without replacing the saved file");
	command(SceneCommand::DismissError); unsupported.Destroy(); scene.CleanupDestroyedObjects();
	command(SceneCommand::Play); scene.CreateGameObject("runtime backup mutation"); std::filesystem::remove(path); command(SceneCommand::Stop);
	Check(Access::Editing(app) && SceneSerializer::Serialize(scene) == beforeFailure && Access::Dirty(app) && Access::Editor(app).dialog == SceneDialog::Error,
		"Missing restore file falls back to pre-play backup and marks it unsaved");
	command(SceneCommand::DismissError); command(SceneCommand::Save);
	Check(std::filesystem::exists(path) && !Access::Dirty(app), "Saving repairs the missing scene file after backup recovery");
	command(SceneCommand::Play); scene.CreateGameObject("retain on double failure"); const auto retained = SceneSerializer::Serialize(scene);
	Access::BreakBackup(app); std::filesystem::remove(path); command(SceneCommand::Stop);
	Check(Access::Editing(app) && SceneSerializer::Serialize(scene) == retained && Access::Editor(app).dialog == SceneDialog::Error && Access::Dirty(app),
		"If file and backup restoration fail, the current scene remains editable");
	command(SceneCommand::DismissError);
}
}
int main(int argc, char** argv)
{
	try
	{
		const auto out = std::filesystem::absolute(argc > 1 ? argv[1] : "x64/SerializationTests"); std::filesystem::create_directories(out);
		// Shell launchers can supply a lower-cased working directory even when
		// importers retain the filesystem's original casing in texture paths.
		const auto project = std::filesystem::current_path(); auto folder = project.filename().wstring();
		std::transform(folder.begin(), folder.end(), folder.begin(), [](wchar_t c) { return std::towlower(c); });
		std::filesystem::current_path(project.parent_path() / folder);
		App app; AppSceneTestAccess::ReleaseCursor(app); ShowWindow(AppSceneTestAccess::WindowOf(app), SW_HIDE); ImGui::GetIO().IniFilename = nullptr;
		TestApp(app, out); std::cout << "PASS " << checks << " editor scene workflow checks\n"; return 0;
	}
	catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
