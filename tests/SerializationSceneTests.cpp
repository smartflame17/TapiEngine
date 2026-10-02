#include "../Scene/GameObject.h"
#include "../Serialization/JsonMath.h"
#include <iostream>
#include <unordered_set>

namespace
{
int checks = 0;
void Check(bool condition, const char* message)
{
	++checks;
	if (!condition) throw std::runtime_error(message);
}
template<typename Fn> void ExpectRejected(Fn&& operation, const char* message)
{
	bool rejected = false;
	try { operation(); }
	catch (const std::invalid_argument&) { rejected = true; }
	Check(rejected, message);
}
void TestSceneFoundation()
{
	static_assert(!std::is_copy_constructible_v<Component>);
	static_assert(!std::is_copy_constructible_v<GameObject>);
	Scene scene;
	Check(scene.GetName() == "Scene", "Default scene name is exposed");
	scene.SetName("Serialization fixture");
	Check(scene.GetName() == "Serialization fixture", "Scene name setter");
	Scene other("Other scene");
	Check(other.GetName() == "Other scene", "Authored scene name is exposed");

	auto& root = scene.CreateGameObject("root");
	auto& child = scene.CreateChildGameObject(root, "child");
	auto& component = child.AddComponent<Component>();
	auto& second = other.CreateGameObject("second");
	std::unordered_set<Guid> identities{ root.GetId(), child.GetId(), component.GetId(), second.GetId() };
	Check(identities.size() == 4 && !identities.contains(Guid{}), "Object and component identities are globally distinct");
	const auto original = root.GetId();
	root.SetName("renamed"); root.SetPosition(1, 2, 3); root.SetStatic(true);
	Check(root.GetId() == original, "Ordinary editing preserves object identity");

	const auto savedObject = Guid::FromString("219ef7b3-8b18-44de-9938-d7126ad24306");
	const auto savedComponent = Guid::FromString("a283df36-66aa-40b8-b65a-b3587d817273");
	root.SetId(nlohmann::json::parse(nlohmann::json(savedObject).dump()).get<Guid>());
	component.SetId(nlohmann::json::parse(nlohmann::json(savedComponent).dump()).get<Guid>());
	Check(root.GetId() == savedObject && component.GetId() == savedComponent, "Saved UUIDs can be restored on objects and components");
	Check(&component.GetGameObject() == &child && child.GetParent() == &root, "Restoring IDs preserves owner and parent relationships");
	ExpectRejected([&] { root.SetId(Guid{}); }, "Object cannot acquire a null identity");
	ExpectRejected([&] { component.SetId(Guid{}); }, "Component cannot acquire a null identity");
	Check(root.GetId() == savedObject && component.GetId() == savedComponent, "Rejected identity assignment leaves saved IDs intact");

	const auto childId = child.GetId();
	const auto componentId = component.GetId();
	auto detached = root.DetachChild(child);
	root.AddChild(std::move(detached));
	Check(child.GetId() == childId && component.GetId() == componentId, "Reparenting preserves object and component UUIDs");
	const auto removed = component.GetId();
	scene.QueueComponentRemoval(component); scene.CleanupPendingComponentRemovals();
	auto& replacement = child.AddComponent<Component>();
	Check(!replacement.GetId().IsNull() && replacement.GetId() != removed, "Replacement component has a fresh UUID");
	scene.Clear();
	Check(scene.GetRootObjects().empty() && scene.GetName() == "Serialization fixture", "Clearing a scene retains its authored name");
	auto& recreated = scene.CreateGameObject("root");
	Check(recreated.GetId() != savedObject && recreated.GetId() != original, "New objects after clearing receive fresh UUIDs");
}
}

int main()
{
	try
	{
		TestSceneFoundation();
		std::cout << "PASS " << checks << " serialization scene foundation checks\n";
		return 0;
	}
	catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
