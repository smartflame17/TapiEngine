#include "../Serialization/SceneSerializer.h"
#include "../Serialization/JsonMath.h"
#include "../Serialization/AssetPath.h"
#include "../Scene/GameObject.h"
#include "../Components/Scripts/ScriptTest.h"
#include "SerializationScriptFixtures.h"
#include "PhysicsTestAccess.h"
#include "../Graphics/Graphics.h"
#include "../Components/Rigidbody.h"
#include "../Components/Collider.h"
#include <box3d/box3d.h>
#include <objbase.h>
#include <iostream>
#include <limits>

using nlohmann::json;
namespace
{
int checks = 0;
void Check(bool value, const char* message) { ++checks; if (!value) throw std::runtime_error(message); }
void Success(const LoadResult& result) { if (!result) throw std::runtime_error(result.errors[0].jsonPath + ": " + result.errors[0].message); ++checks; }
void Tick(Scene& scene) { scene.ProcessScriptAwakeAndStart(true); scene.Update(1.0f / 60, true); }
void TestScripts(Graphics& gfx, const std::filesystem::path& out)
{
	Check(AssetPath::Reference(L"C:/Virtual/Project/Models/Character.fbx", L"c:/virtual/project") == "Models/Character.fbx",
		"Windows project containment ignores casing and preserves asset spelling without disk access");
	bool outsideRejected = false;
	try { AssetPath::Reference(L"C:/Virtual/ProjectOther/Character.fbx", L"c:/virtual/project"); }
	catch (const std::invalid_argument&) { outsideRejected = true; }
	Check(outsideRejected, "Case-insensitive containment still rejects sibling directories");
	Scene source("script fixture"); auto& object = source.CreateGameObject("script owner");
	auto& script = object.AddComponent<SerializationProbe>();
	script.integer = -42; script.number = 8.5f; script.text = "saved UTF-8 문자열"; script.vector[1] = -4; script.color[2] = 0.9f; script.flag = false;
	script.SetEnabled(false);
	object.AddComponent<Rigidbody>(); object.AddComponent<Collider>();
	Check(script.GetScriptName() == "SerializationProbe", "AddComponent resolves registered script names");
	const auto document = SceneSerializer::Serialize(source);
	Check(script.properties.size() == 6 && SceneSerializer::Serialize(source) == document, "Exposure metadata is cleared on every save");
	const auto path = out / "scripts.scene"; Success(SceneSerializer::SaveToFile(source, path));
	Scene restored; LoadContext context{ gfx }; Success(SceneSerializer::LoadFromFile(path, restored, context));
	Check(SceneSerializer::Serialize(restored) == document, "All six exposed types, UUIDs, and disabled state round trip");
	SerializationProbe::ResetCounts();
	auto* loaded = restored.GetRootObjects()[0]->GetComponent<SerializationProbe>();
	Tick(restored); Check(SerializationProbe::starts == 0, "Disabled script waits for activation");
	loaded->SetEnabled(true);
	Check(SerializationProbe::enables == 0, "Enabling in Edit mode does not call lifecycle hooks");
	restored.ProcessScriptAwakeAndStart(false);
	Check(SerializationProbe::starts == 0, "Edit and paused lifecycle processing is inert");
	Tick(restored); Tick(restored);
	Check(SerializationProbe::awakes == 1 && SerializationProbe::starts == 1 && SerializationProbe::enables == 1 && SerializationProbe::updates == 2,
		"Loaded script activates once on normal running ticks");
	loaded->SetEnabled(false); restored.ProcessScriptAwakeAndStart(false);
	Check(SerializationProbe::disables == 0, "Disable callbacks wait until simulation resumes");
	Tick(restored); loaded->SetEnabled(true); Tick(restored);
	Check(SerializationProbe::disables == 1 && SerializationProbe::enables == 2 && SerializationProbe::starts == 1,
		"Re-enabling a started script does not repeat initial lifecycle");
	for (int i = 0; i != 5; ++i)
	{
		Success(SceneSerializer::Deserialize(document, restored, context));
		SerializationProbe::ResetCounts(); loaded = restored.GetRootObjects()[0]->GetComponent<SerializationProbe>(); loaded->SetEnabled(true); Tick(restored);
		Check(SerializationProbe::starts == 1 && SerializationProbe::updates == 1, "Repeated load registers one instance exactly once");
	}
	auto compatible = document;
	auto& fields = compatible["scene"]["objects"][0]["components"][0]["data"]["fields"];
	fields.erase("integer"); fields["removed field"] = { { "type", "former_type" }, { "value", "obsolete" } };
	const auto warnings = SceneSerializer::Deserialize(compatible, restored, context); Success(warnings);
	Check(warnings.warnings.size() == 1 && warnings.warnings[0].objectName == "script owner" && warnings.warnings[0].componentId.has_value() &&
		warnings.warnings[0].jsonPath.find("removed field") != std::string::npos, "Unknown field warning carries complete location context");
	Check(restored.GetRootObjects()[0]->GetComponent<SerializationProbe>()->integer == 17, "Missing fields preserve compiled defaults");
	const auto before = SceneSerializer::Serialize(restored); const auto maps = context.gameObjects;
	const auto bodies = b3World_GetCounters(PhysicsTestAccess::World(Physics::GetInstance())).bodyCount;
	auto reject = [&](json invalid, const char* message) {
		const auto result = SceneSerializer::Deserialize(invalid, restored, context);
		Check(!result && !result.errors[0].jsonPath.empty(), message);
		Check(SceneSerializer::Serialize(restored) == before && context.gameObjects == maps, "Failed script loading preserves scene and lookups");
		Check(b3World_GetCounters(PhysicsTestAccess::World(Physics::GetInstance())).bodyCount == bodies, "Failed candidate loading releases its physics registrations");
	};
	auto invalid = document; auto* data = &invalid["scene"]["objects"][0]["components"][0]["data"];
	(*data)["className"] = "MissingScript"; reject(invalid, "Missing class fails clearly");
	invalid = document; data = &invalid["scene"]["objects"][0]["components"][0]["data"];
	(*data)["fields"]["integer"]["value"] = 2147483648ULL; reject(invalid, "Overflowing int is rejected");
	(*data)["fields"]["integer"]["value"] = 1.5; reject(invalid, "Fractional int is rejected");
	invalid = document; data = &invalid["scene"]["objects"][0]["components"][0]["data"];
	(*data)["fields"]["number"]["value"] = (std::numeric_limits<double>::infinity)(); reject(invalid, "Nonfinite float is rejected");
	(*data)["fields"]["number"]["value"] = 1e100; reject(invalid, "Out-of-range float is rejected");
	invalid = document; data = &invalid["scene"]["objects"][0]["components"][0]["data"];
	(*data)["fields"]["vector"]["type"] = "color"; reject(invalid, "Color and Vector3 are distinct types");
	(*data)["fields"]["vector"]["type"] = "vector3"; (*data)["fields"]["vector"]["value"] = json::array({1,2}); reject(invalid, "Vector length is validated");
	invalid = document; data = &invalid["scene"]["objects"][0]["components"][0]["data"];
	(*data)["enabled"] = 1; reject(invalid, "Enabled must be a boolean");
	invalid = document;
	auto throwing = invalid["scene"]["objects"][0]["components"][0]; throwing["id"] = Guid::Generate();
	throwing["data"] = { { "className", "ThrowingSerializationScript" } };
	invalid["scene"]["objects"][0]["components"].push_back(throwing);
	reject(invalid, "Throwing constructors are caught after another candidate script exists");
	SerializationProbe::ResetCounts(); loaded = restored.GetRootObjects()[0]->GetComponent<SerializationProbe>(); loaded->SetEnabled(true); Tick(restored);
	Check(SerializationProbe::starts == 1 && SerializationProbe::updates == 1, "Discarded candidates never enter active lifecycle queues");
	for (bool nullMode : { false, true })
	{
		SerializationProbe::duplicate = !nullMode; SerializationProbe::nullPointer = nullMode;
		Check(!SceneSerializer::Deserialize(document, restored, context), "Invalid compiled exposure metadata rejects candidate loading");
		Check(!SceneSerializer::SaveToFile(source, path), "Invalid compiled exposure metadata rejects saving");
		SerializationProbe::duplicate = SerializationProbe::nullPointer = false;
	}
	Scene testScene; auto& test = testScene.CreateGameObject("ScriptTest").AddComponent<ScriptTest>(nullptr);
	test.testInt = 23; test.testFloat = 3.5f; test.exposedString = "saved"; test.SetEnabled(false);
	const auto testDocument = SceneSerializer::Serialize(testScene); Success(SceneSerializer::Deserialize(testDocument, restored, context));
	Check(SceneSerializer::Serialize(restored) == testDocument, "Production ScriptTest values and enabled state round trip");
	class Unregistered : public CustomBehaviour {};
	testScene.CreateGameObject("unregistered").AddComponent<Unregistered>();
	Check(!SceneSerializer::SaveToFile(testScene, path), "Unregistered script classes fail saving");
}
}
int main(int argc, char** argv)
{
	try
	{
		const auto com = CoInitializeEx(nullptr, COINIT_MULTITHREADED); if (FAILED(com)) throw std::runtime_error("COM initialization failed");
		const auto out = std::filesystem::path(argc > 1 ? argv[1] : "x64/SerializationTests"); std::filesystem::create_directories(out);
		WNDCLASSW wc{}; wc.lpfnWndProc = DefWindowProcW; wc.hInstance = GetModuleHandle(nullptr); wc.lpszClassName = L"SerializationScriptsHidden"; RegisterClassW(&wc);
		const auto window = CreateWindowW(wc.lpszClassName, L"Script tests", WS_OVERLAPPEDWINDOW, 0, 0, 800, 600, nullptr, nullptr, wc.hInstance, nullptr);
		ImGui::CreateContext(); ImGui::GetIO().IniFilename = nullptr;
		{ auto physics = PhysicsTestAccess::Create(); Graphics graphics(window, 800, 600); graphics.DisableImGui(); TestScripts(graphics, out); }
		ImGui::DestroyContext(); DestroyWindow(window); CoUninitialize();
		std::cout << "PASS " << checks << " script serialization checks\n"; return 0;
	}
	catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
