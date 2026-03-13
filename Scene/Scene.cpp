#include "Scene.h"
#include "GameObject.h"
#include "../Components/DrawableComponent.h"

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
	for (const auto& drawable : drawables)
	{
		drawable->OnRender(gfx);
	}
}

void Scene::RegisterDrawable(DrawableComponent* drawable) noexcept
{
    drawables.push_back(drawable);
}

void Scene::UnregisterDrawable(DrawableComponent* drawable) noexcept
{
    auto it = std::find(drawables.begin(), drawables.end(), drawable);
    if (it != drawables.end())
    {
        drawables.erase(it);
    }
}

void Scene::DrawHierarchyWindow() noexcept
{
	ImGui::SetNextWindowSize(ImVec2(300, 720), ImGuiCond_Always);
	ImGui::SetNextWindowPos(ImVec2(0, 60), ImGuiCond_Always);
	if (!ImGui::Begin(name.c_str(), nullptr,
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoCollapse
		))
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


GameObject* Scene::GetSelectedObject() const noexcept
{
	return selectedObject;
}

void Scene::DrawInspectorWindow() noexcept
{
	ImGui::SetNextWindowSize(ImVec2(340, 800), ImGuiCond_Always);
	ImGui::SetNextWindowPos(ImVec2(1580, 15), ImGuiCond_Always);
	if (!ImGui::Begin("Inspector", nullptr,
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoCollapse
		))
	{
		ImGui::End();
		return;
	}

	if (selectedObject == nullptr)
	{
		ImGui::TextUnformatted("No GameObject selected.");
		ImGui::End();
		return;
	}

	ImGui::Text("GameObject: %s", selectedObject->GetName().c_str());
	ImGui::Text("ID: %llu", static_cast<unsigned long long>(selectedObject->GetId()));
	ImGui::Separator();

	const auto& components = selectedObject->GetComponents();
	if (components.empty())
	{
		ImGui::TextUnformatted("No components.");
	}
	else
	{
		for (const auto& component : components)
		{
			if (component != nullptr)
			{
				component->OnInspector();
				ImGui::Spacing();
			}
		}
	}

	ImGui::End();
}
const std::vector<std::unique_ptr<GameObject>>& Scene::GetRootObjects() const noexcept
{
	return rootObjects;
}

void Scene::DrawHierarchyNode(GameObject& object) noexcept
{
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_DrawLinesToNodes;
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
