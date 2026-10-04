#include "../Serialization/SceneSerializer.h"
#include "../Serialization/ComponentRegistry.h"
#include "../Serialization/AssetPath.h"
#include "../Serialization/JsonEnums.h"
#include "../Serialization/JsonMath.h"
#include "../Scene/GameObject.h"
#include "../Graphics/Camera.h"
#include "../Graphics/Lighting/PointLight.h"
#include "../Graphics/Lighting/SpotLight.h"
#include "../Graphics/Lighting/DirectionalLight.h"
#include "../Graphics/Drawable/Model.h"
#include "../Components/Animator.h"
#include "../Components/AudioClip.h"
#include "../Physics/Physics.h"
#include "PhysicsComponentTestAccess.h"
#include "PhysicsTestAccess.h"
#include <box3d/box3d.h>
#include <objbase.h>
#include <fstream>
#include <iostream>

using nlohmann::json;
namespace dx = DirectX;
namespace
{
int checks = 0;
void Check(bool value, const char* message)
{
	++checks; if (!value) throw std::runtime_error(message);
}
void CheckResult(const LoadResult& result)
{
	if (!result) throw std::runtime_error(result.errors.front().jsonPath + ": " + result.errors.front().message);
	++checks;
}
json Document(json objects, const char* name = "fixture")
{
	return { { "format", "TapiScene" }, { "version", 1 }, { "scene", { { "name", name }, { "objects", std::move(objects) } } } };
}
json Object(const char* name)
{
	return { { "id", Guid::Generate() }, { "name", name }, { "static", false }, { "transform", Transform{} },
		{ "components", json::array() }, { "children", json::array() } };
}
json ComponentRecord(const char* type, json data)
{
	return { { "id", Guid::Generate() }, { "type", type }, { "data", std::move(data) } };
}
float MatrixError(dx::FXMMATRIX a, dx::CXMMATRIX b)
{
	dx::XMFLOAT4X4 lhs, rhs; dx::XMStoreFloat4x4(&lhs, a); dx::XMStoreFloat4x4(&rhs, b);
	float result = 0;
	for (int i = 0; i < 4; ++i) for (int j = 0; j < 4; ++j) result = (std::max)(result, std::abs(lhs.m[i][j] - rhs.m[i][j]));
	return result;
}
void TestHierarchy(Graphics& graphics, const std::filesystem::path& out)
{
	Scene empty("empty"); Scene restored;
	LoadContext context{ graphics };
	const auto emptyData = SceneSerializer::Serialize(empty);
	CheckResult(SceneSerializer::Deserialize(emptyData, restored, context));
	Check(SceneSerializer::Serialize(restored) == emptyData, "Empty scene round trip");

	Scene scene("Nested hierarchy");
	auto& root = scene.CreateGameObject("root");
	root.SetPosition(3, -2, 8); root.SetRotation(0.3f, -0.9f, 1.2f); root.SetScale(2, 3, 4); root.SetStatic(true);
	auto& child = scene.CreateChildGameObject(root, "child");
	child.SetPosition(-1, 2, -3); child.SetRotation(0.8f, 0.1f, -0.4f); child.SetScale(-1, 0.5f, 2);
	auto& grandchild = scene.CreateChildGameObject(child, "grandchild"); grandchild.SetPosition(1, 2, 3);
	scene.CreateGameObject("second root");
	const auto saved = SceneSerializer::Serialize(scene);
	Check(SceneSerializer::SerializeGameObject(root) == saved["scene"]["objects"][0], "Public subtree writer uses the same recursive scene schema");
	CheckResult(SceneSerializer::Deserialize(json::parse(saved.dump()), restored, context));
	Check(SceneSerializer::Serialize(restored) == saved && context.gameObjects.size() == 4, "Nested hierarchy and UUIDs round trip");
	Check(MatrixError(grandchild.GetWorldTransformMatrix(), context.gameObjects.at(grandchild.GetId())->GetWorldTransformMatrix()) < 1e-5f,
		"Nested local and world transforms are preserved");
	Check(context.gameObjects.at(child.GetId())->GetParent() == context.gameObjects.at(root.GetId()), "Parents are restored independently of saved pointers");
	const auto sceneFile = out / "stage2.scene";
	CheckResult(SceneSerializer::SaveToFile(scene, sceneFile));
	CheckResult(SceneSerializer::LoadFromFile(sceneFile, restored, context));
	scene.SetName("Replacement");
	CheckResult(SceneSerializer::SaveToFile(scene, sceneFile));
	CheckResult(SceneSerializer::LoadFromFile(sceneFile, restored, context));
	Check(restored.GetName() == "Replacement", "Atomic save replaces an existing file");
	bool temporaryFound = false;
	for (const auto& file : std::filesystem::directory_iterator(out))
		temporaryFound |= file.path().filename().string().find(".tmp.") != std::string::npos;
	Check(!temporaryFound, "Successful saves leave no temporary files");

	const auto original = SceneSerializer::Serialize(restored);
	const auto* originalObject = restored.GetRootObjects()[0].get();
	auto malformed = original; malformed["scene"]["objects"][0]["children"][0]["transform"]["scale"] = { 1, 2 };
	const auto failure = SceneSerializer::Deserialize(malformed, restored, context);
	Check(!failure && failure.errors[0].jsonPath == "$.scene.objects[0].children[0].transform", "Malformed transform reports its JSON path");
	Check(SceneSerializer::Serialize(restored) == original && restored.GetRootObjects()[0].get() == originalObject,
		"Structural validation failure leaves the active hierarchy intact");
	malformed = original; malformed["scene"]["objects"][1]["id"] = malformed["scene"]["objects"][0]["id"];
	const auto duplicate = SceneSerializer::Deserialize(malformed, restored, context);
	Check(!duplicate && duplicate.errors[0].objectId.has_value(), "Duplicate UUIDs are rejected with object context");
	Check(SceneSerializer::Serialize(restored) == original, "Duplicate validation does not clear the scene");
	malformed = original; malformed["version"] = 99;
	Check(!SceneSerializer::Deserialize(malformed, restored, context), "Unsupported version is rejected");
	std::ofstream(sceneFile, std::ios::trunc) << "{malformed";
	Check(!SceneSerializer::LoadFromFile(sceneFile, restored, context) && SceneSerializer::Serialize(restored) == original,
		"Malformed scene file does not mutate the scene");
}
void TestBasicComponents(Graphics& graphics)
{
	Scene scene("basic components");
	LoadContext createContext{ graphics };
	const auto& registry = ComponentRegistry::Builtins();
	auto& root = scene.CreateGameObject("settings");
	registry.Create("tapi.camera", root, { { "speed", 3.5f }, { "sensitivity", 0.006f } }, createContext);
	registry.Create("tapi.point_light", root, { { "color", { 0.1f, 0.2f, 0.3f } }, { "intensity", 2.5f }, { "gizmoRadius", 0.7f },
		{ "attConstant", 0.5f }, { "attLinear", 0.02f }, { "attQuadratic", 0.009f } }, createContext);
	registry.Create("tapi.spot_light", root, { { "color", { 0.5f, 0.6f, 0.7f } }, { "intensity", 0.8f }, { "gizmoRadius", 0.6f },
		{ "innerAngle", 0.2f }, { "outerAngle", 0.8f } }, createContext);
	registry.Create("tapi.directional_light", root, { { "color", { 0.9f, 0.8f, 0.7f } }, { "intensity", 0.33f } }, createContext);
	auto& physical = scene.CreateChildGameObject(root, "physics");
	physical.SetPosition(4, 5, 6);
	// Deliberately authored in Collider-before-Rigidbody order.
	registry.Create("tapi.collider", physical, { { "shape", "capsule" }, { "center", { 0.1f, 0.2f, 0.3f } }, { "size", { 1, 2, 3 } },
		{ "radius", 0.4f }, { "height", 2.3f }, { "density", 2.5f }, { "friction", 0.3f }, { "restitution", 0.2f } }, createContext);
	registry.Create("tapi.rigidbody", physical, { { "bodyType", "dynamic" }, { "gravityScale", 0.5f }, { "linearDamping", 1.25f },
		{ "angularDamping", 0.75f }, { "motionLocks", { { "linearX", true }, { "angularZ", true } } } }, createContext);
	const auto saved = SceneSerializer::Serialize(scene);
	Scene restored; LoadContext context{ graphics };
	CheckResult(SceneSerializer::Deserialize(saved, restored, context));
	Check(SceneSerializer::Serialize(restored) == saved && context.components.size() == 6, "All basic components preserve their authored metadata and IDs");
	auto* restoredPhysics = context.gameObjects.at(physical.GetId());
	Check(restoredPhysics->GetComponents()[0]->GetType() == ComponentType::Collider, "Authored component order is preserved after dependency-ordered construction");
	auto* body = restoredPhysics->GetComponent<Rigidbody>(); auto* collider = restoredPhysics->GetComponent<Collider>();
	Check(b3Body_IsValid(PhysicsComponentTestAccess::Body(*body)) && b3Shape_IsValid(PhysicsComponentTestAccess::Shape(*collider)), "Physics resources are reconstructed via AddComponent");
	Check(body->GetMotionLocks().linearX && body->GetMotionLocks().angularZ && collider->GetDensity() == 2.5f, "Physics metadata is restored");

	auto malformed = saved;
	malformed["scene"]["objects"][0]["components"][0]["data"]["speed"] = -1;
	const auto invalid = SceneSerializer::Deserialize(malformed, restored, context);
	Check(!invalid && invalid.errors[0].componentType == "tapi.camera" && invalid.errors[0].jsonPath.ends_with(".data.speed"), "Component metadata errors include the component and field path");
	Check(SceneSerializer::Serialize(restored) == saved, "Invalid component metadata is rejected before reconstruction");
	malformed = saved; malformed["scene"]["objects"][0]["components"][0]["id"] = malformed["scene"]["objects"][0]["id"];
	Check(!SceneSerializer::Deserialize(malformed, restored, context), "UUID uniqueness includes both objects and components");
	malformed = saved; malformed["scene"]["objects"][0]["components"][0]["type"] = "unknown.component";
	Check(!SceneSerializer::Deserialize(malformed, restored, context), "Unknown component types are rejected");
	Scene defaults; const auto defaultRoot = Object("defaults"); auto defaultsData = Document(json::array({ defaultRoot }));
	defaultsData["scene"]["objects"][0]["components"].push_back(ComponentRecord("tapi.camera", json::object()));
	CheckResult(SceneSerializer::Deserialize(defaultsData, defaults, context));
	Check(defaults.GetRootObjects()[0]->GetComponent<Camera>()->camSpeed == 0.1f, "Missing optional metadata retains compiled defaults");
}

void TestPrimitive(Graphics& graphics)
{
	Scene scene("primitive"); LoadContext create{ graphics };
	auto& object = scene.CreateGameObject("textured plane");
	const auto& registry = ComponentRegistry::Builtins();
	registry.Create("tapi.drawable", object, { { "kind", "primitive" }, { "shape", "plane" }, { "surface", "textured" },
		{ "texture", "Graphics/Textures/obama.jpg" }, { "normalMap", "Graphics/Textures/obama.jpg" }, { "normalMapEnabled", true }, { "sampler", "point_clamp" },
		{ "material", { { "color", { 0.3f, 0.4f, 0.5f } }, { "specularColor", { 0.1f, 0.2f, 0.3f } }, { "specularIntensity", 0.7f }, { "specularPower", 64 } } } }, create);
	auto* primitive = dynamic_cast<Primitive*>(object.GetComponent<DrawableComponent>()->GetDrawable());
	Transform local; local.position = { 1, 2, 3 }; local.rotation = { 0.1f, 0.2f, 0.3f }; local.scale = { 2, 3, 4 }; primitive->SetTransform(local);
	const auto saved = SceneSerializer::Serialize(scene);
	Scene restored; LoadContext context{ graphics };
	CheckResult(SceneSerializer::Deserialize(saved, restored, context));
	Check(SceneSerializer::Serialize(restored) == saved, "Primitive reconstruction preserves material, textures, sampler, normal-map state and local transform");
	const auto& data = saved["scene"]["objects"][0]["components"][0]["data"];
	Check(!data.contains("vertices") && !data.contains("encodedImage") && !data["material"].contains("padding0"), "Primitive JSON contains metadata only");
}

struct RecordingLoader : AssetLoader
{
	int models = 0, animations = 0, textures = 0;
	std::shared_ptr<const ModelAsset> LoadModel(const std::filesystem::path& path) override { ++models; return AssetLoader::Default().LoadModel(path); }
	std::vector<std::shared_ptr<const Animation::AnimationClip>> LoadAnimations(const std::filesystem::path& path) override { ++animations; return AssetLoader::Default().LoadAnimations(path); }
	std::shared_ptr<const TextureAsset> LoadTexture(const std::filesystem::path& path) override { ++textures; return AssetLoader::Default().LoadTexture(path); }
};
void TestHumanoid(Graphics& graphics)
{
	Scene scene("humanoid"); auto& object = scene.CreateGameObject("rigged figure"); object.SetScale(0.007f, 0.007f, 0.007f);
	object.AddComponent<DrawableComponent>(std::make_unique<Model>(graphics, "Graphics/Models/Humanoid/Body.fbx"));
	auto& animator = object.AddComponent<Animator>();
	Check(animator.LoadAnimations("Graphics/Models/Humanoid/Animations/Walking.fbx", true), "Walking animation imports");
	Check(animator.LoadAnimations("Graphics/Models/Humanoid/Animations/Jump.fbx", false), "Jump animation imports");
	animator.SelectClip(1); animator.SetSpeed(1.75f); animator.Play(); animator.Seek(0.37); scene.UpdateAnimations(0, false, false);
	auto saved = SceneSerializer::Serialize(scene);
	// Serialized order is allowed to put Animator before its target resource.
	auto& components = saved["scene"]["objects"][0]["components"]; std::swap(components[0], components[1]);
	RecordingLoader loader; Scene restored; LoadContext context{ graphics }; context.assets = &loader;
	CheckResult(SceneSerializer::Deserialize(json::parse(saved.dump()), restored, context));
	Check(SceneSerializer::Serialize(restored) == saved, "Existing humanoid model and animation setup survives a scene round trip");
	Check(loader.models == 1 && loader.animations == 2, "Model and animation reconstruction uses the injected resource loader");
	auto* restoredAnimator = context.gameObjects.at(object.GetId())->GetComponent<Animator>();
	Check(restoredAnimator->GetState() == Animation::PlaybackState::Stopped && restoredAnimator->GetTime() == 0, "Runtime animation playback state and time are rebuilt");
	Check(restoredAnimator->GetSelectedClip() == 1 && restoredAnimator->GetSpeed() == 1.75f && !restoredAnimator->GetClips()[1].loop,
		"Selected clip, speed and per-clip loop metadata are preserved");
	const auto& modelData = components[1]["data"];
	Check(modelData["asset"] == "Graphics/Models/Humanoid/Body.fbx" && !modelData.contains("skeleton") && !modelData.contains("meshes"), "Model JSON saves a relative reference without imported geometry or skeleton data");
	Check(!components[0]["data"].contains("pose") && !components[0]["data"].contains("time"), "Animator JSON excludes evaluated pose and playback time");

	const auto original = SceneSerializer::Serialize(restored);
	auto missing = saved; missing["scene"]["objects"][0]["components"][1]["data"]["asset"] = "Graphics/Models/__missing__.fbx";
	const auto failure = SceneSerializer::Deserialize(missing, restored, context);
	Check(!failure && failure.errors[0].jsonPath.ends_with(".data.asset") && SceneSerializer::Serialize(restored) == original,
		"Resource construction failure releases the detached hierarchy and preserves the active scene");
}
std::shared_ptr<ModelAsset> FixtureModel()
{
	auto asset = std::make_shared<ModelAsset>();
	Animation::SkeletonNode root; root.name = "root"; root.meshes = { 0 }; asset->skeleton.nodes = { root };
	MeshAsset mesh; mesh.name = "fixture"; mesh.hasUV = false;
	mesh.vertices = { {{0,0,0},{0,0,-1},{1,0,0},{0,0},{}}, {{0,1,0},{0,0,-1},{1,0,0},{0,1},{}}, {{1,0,0},{0,0,-1},{1,0,0},{1,0},{}} };
	mesh.indices = { 0, 1, 2 }; mesh.bounds.Center = { .5f, .5f, 0 }; mesh.bounds.Extents = { .5f, .5f, .01f }; asset->meshes.push_back(mesh);
	return asset;
}
void TestResourceSwap(Graphics& graphics)
{
	struct MemoryLoader : RecordingLoader
	{
		std::shared_ptr<ModelAsset> resource = FixtureModel();
		std::shared_ptr<const TextureAsset> image = AssetLoader::Default().LoadTexture("Graphics/Textures/obama.jpg");
		std::shared_ptr<const ModelAsset> LoadModel(const std::filesystem::path&) override { ++models; return resource; }
		std::shared_ptr<const TextureAsset> LoadTexture(const std::filesystem::path& path) override
		{
			if (path != resource->meshes[0].baseColorTexture && path != resource->meshes[0].normalTexture)
				throw std::runtime_error("Unexpected virtual texture reference.");
			++textures; return image;
		}
	} loader;
	Scene scene("injected resources"); LoadContext context{ graphics }; context.assets = &loader;
	context.projectRoot = std::filesystem::absolute(AssetPath::FromUtf8("virtual/프로젝트"));
	loader.resource->meshes[0].hasUV = true;
	loader.resource->meshes[0].baseColorTexture = context.projectRoot / AssetPath::FromUtf8("textures/색상.jpg");
	loader.resource->meshes[0].normalTexture = context.projectRoot / AssetPath::FromUtf8("textures/법선.jpg");
	auto object = Object("model from resource data");
	json metadata = { { "kind", "model" }, { "asset", "virtual/fixture.model" } };
	metadata["nodeOverrides"] = json::array();
	metadata["nodeOverrides"].push_back({ { "node", 0 }, { "transform", Transform{ {1,2,3},{0,0,0},{2,2,2} } } });
	metadata["meshOverrides"] = json::array();
	metadata["meshOverrides"].push_back({ { "instance", 0 }, { "material", { { "color", { 0.2f, 0.3f, 0.4f } } } } });
	object["components"].push_back(ComponentRecord("tapi.drawable", std::move(metadata)));
	CheckResult(SceneSerializer::Deserialize(Document(json::array({ object })), scene, context));
	auto* model = dynamic_cast<Model*>(scene.GetRootObjects()[0]->GetComponent<DrawableComponent>()->GetDrawable());
	Check(loader.models == 1 && model->GetAsset() == loader.resource && model->GetNodeTransforms()[0].position.x == 1,
		"Reconstruction accepts resource data supplied for a reference that has no physical file");
	Check(loader.textures >= 2 && model->GetMeshInstance(0).GetMaterial().useNormalMap == 1,
		"Virtual texture bytes create real GPU resources through the injected loader");
	SerializationContext saveContext{ context.projectRoot };
	const auto saved = SceneSerializer::Serialize(scene, saveContext);
	Check(saved["scene"]["objects"][0]["components"][0]["data"]["meshOverrides"][0]["texture"] == "textures/색상.jpg",
		"Texture references preserve UTF-8 under a virtual project root");
	Scene restored; LoadContext second{ graphics }; second.assets = &loader;
	second.projectRoot = context.projectRoot;
	CheckResult(SceneSerializer::Deserialize(saved, restored, second));
	Check(SceneSerializer::Serialize(restored, saveContext) == saved, "Authored node and mesh overrides are metadata and survive reconstruction");
}
void TestScope(Graphics& graphics, const std::filesystem::path& out)
{
	Scene scene("supported"); scene.CreateGameObject("root");
	const auto path = out / "supported.scene";
	CheckResult(SceneSerializer::SaveToFile(scene, path));
	std::ifstream beforeFile(path); const std::string before((std::istreambuf_iterator<char>(beforeFile)), {}); beforeFile.close();
	scene.GetRootObjects()[0]->AddComponent<AudioClip>();
	Check(!SceneSerializer::SaveToFile(scene, path), "Unsupported component payloads are reported instead of silently omitted");
	std::ifstream afterFile(path); const std::string after((std::istreambuf_iterator<char>(afterFile)), {});
	Check(before == after, "Serialization failure preserves the previous scene file");
	auto object = Object("script"); object["components"].push_back(ComponentRecord("tapi.script", json::object()));
	Scene restored; LoadContext context{ graphics };
	Check(!SceneSerializer::Deserialize(Document(json::array({ object })), restored, context), "Script metadata requires a registered class name");
}
}

int main(int argc, char** argv)
{
	try
	{
		const auto initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		if (FAILED(initialized)) throw std::runtime_error("Cannot initialize COM for texture reconstruction tests.");
		const std::filesystem::path out = argc > 1 ? argv[1] : "x64/SerializationTests"; std::filesystem::create_directories(out);
		WNDCLASSW windowClass{}; windowClass.lpfnWndProc = DefWindowProcW; windowClass.hInstance = GetModuleHandle(nullptr); windowClass.lpszClassName = L"SerializationRoundTripHiddenWindow";
		RegisterClassW(&windowClass);
		const auto window = CreateWindowW(windowClass.lpszClassName, L"Serialization tests", WS_OVERLAPPEDWINDOW, 0, 0, 800, 600, nullptr, nullptr, windowClass.hInstance, nullptr);
		Check(window != nullptr, "Hidden graphics test window is created");
		ImGui::CreateContext(); ImGui::GetIO().IniFilename = nullptr;
		{
			auto physics = PhysicsTestAccess::Create(); Graphics graphics(window, 800, 600); graphics.DisableImGui();
			TestHierarchy(graphics, out); TestBasicComponents(graphics); TestPrimitive(graphics); TestHumanoid(graphics); TestResourceSwap(graphics); TestScope(graphics, out);
		}
		ImGui::DestroyContext(); DestroyWindow(window); CoUninitialize();
		std::cout << "PASS " << checks << " stage 2-4 serialization round-trip checks\n"; return 0;
	}
	catch (const std::exception& failure) { std::cerr << "FAIL: " << failure.what() << '\n'; return 1; }
}
