// Interactive verification harness: the production App and UI, with an authored
// ScriptTest and falling primitive ready for save/load and Play/Stop inspection.
#include "../App.h"
#include "../Components/Scripts/ScriptTest.h"
#include <filesystem>
#include <iostream>

class AppSceneTestAccess
{
public:
	static Scene& SceneOf(App& app) { return app.scene; }
};
int main(int argc, char** argv)
{
	try
	{
		auto root = std::filesystem::absolute(argv[0]);
		for (int i = 0; i < 4; ++i) root = root.parent_path();
		std::filesystem::current_path(root);
		App app; ImGui::GetIO().IniFilename = nullptr;
		for (const auto& object : AppSceneTestAccess::SceneOf(app).GetRootObjects())
			if (object->GetName() == "Material Cube")
			{
				auto& script = object->AddComponent<ScriptTest>(nullptr); script.testInt = 23; script.SetEnabled(false);
				object->AddComponent<Rigidbody>().SetType(Rigidbody::Type::Dynamic); object->AddComponent<Collider>();
			}
		return app.Begin();
	}
	catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
