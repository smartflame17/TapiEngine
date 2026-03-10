#include "Scene.h"
#include "GameObject.h"
#include "../imgui/imgui.h"
#include <cstdint>

GameObject& Scene::CreateGameObject(const std::string& name)
{
	auto object = std::make_unique<GameObject>(*this, name);
	GameObject& objectRef = *object;
	rootObjects.push_back(std::move(object));
	return objectRef;
}

GameObject& Scene::CreateChildGameObject(GameObject& parent, const std::string& name)
{
	auto child = std::make_unique<GameObject>(*this, name);
	return parent.AddChild(std::move(child));
}

void Scene::Clear() noexcept
{
	rootObjects.clear();
	selectedObject = nullptr;
}

void Scene::Update(float dt, bool isSimulationRunning) noexcept
{
	for (auto& object : rootObjects)
	{
		object->Update(dt, isSimulationRunning);
	}
}

void Scene::Render(Graphics& gfx) const noexcept(!IS_DEBUG)
{
	for (const auto& object : rootObjects)
	{
		object->Render(gfx);
	}
}

void Scene::DrawHierarchyWindow() noexcept
{
	if (!ImGui::Begin("Scene Hierarchy"))
	{
		ImGui::End();
		return;
	}

	for (auto& object : rootObjects)
	{
		DrawHierarchyNode(*object);
	}

	ImGui::Separator();
	if (selectedObject != nullptr)
	{
		ImGui::Text("Selected: %s", selectedObject->GetName().c_str());
		ImGui::Text("GameObject ID: %llu", static_cast<unsigned long long>(selectedObject->GetId()));
	}
	else
	{
		ImGui::TextUnformatted("Selected: <none>");
	}

	ImGui::End();
}

const std::vector<std::unique_ptr<GameObject>>& Scene::GetRootObjects() const noexcept
{
	return rootObjects;
}

void Scene::DrawHierarchyNode(GameObject& object) noexcept
{
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
	if (object.GetChildren().empty())
		flags |= ImGuiTreeNodeFlags_Leaf;
	if (selectedObject == &object)
		flags |= ImGuiTreeNodeFlags_Selected;

	const bool opened = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<uintptr_t>(object.GetId())), flags, "%s", object.GetName().c_str());
	if (ImGui::IsItemClicked())
	{
		selectedObject = &object;
	}

	if (opened)
	{
		for (auto& child : object.GetChildren())
		{
			DrawHierarchyNode(*child);
		}
		ImGui::TreePop();
	}
}
