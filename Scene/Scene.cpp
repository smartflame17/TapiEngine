#include "Scene.h"
#include "GameObject.h"
#include "../Components/Component.h"
#include "../Components/DrawableComponent.h"
#include <algorithm>

Scene::Scene() : name("Scene") {}

Scene::Scene(const std::string& sceneName) : name(sceneName) {}

Scene::~Scene() = default;

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

	auto& objectTransform = selectedObject->GetTransform();
	ImGui::Text("Transform");
	ImGui::DragFloat3("Position", &objectTransform.position.x, 0.05f);
	DirectX::XMFLOAT3 rotationDegrees = {
		DirectX::XMConvertToDegrees(objectTransform.rotation.x),
		DirectX::XMConvertToDegrees(objectTransform.rotation.y),
		DirectX::XMConvertToDegrees(objectTransform.rotation.z)
	};
	if (ImGui::DragFloat3("Rotation (degrees)", &rotationDegrees.x, 0.5f))
	{
		objectTransform.rotation.x = DirectX::XMConvertToRadians(rotationDegrees.x);
		objectTransform.rotation.y = DirectX::XMConvertToRadians(rotationDegrees.y);
		objectTransform.rotation.z = DirectX::XMConvertToRadians(rotationDegrees.z);
	}
	ImGui::DragFloat3("Scale", &objectTransform.scale.x, 0.05f, 0.01f, 200.0f, "%.2f");
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

DirectX::XMMATRIX Scene::GetSelectedWorldTransformMatrix() const noexcept
{
	if (selectedObject != nullptr)
	{
		return selectedObject->GetWorldTransformMatrix();
	}
	return DirectX::XMMatrixIdentity();
}

void Scene::SetSelectedWorldTransformMatrix(DirectX::FXMMATRIX matrix) noexcept
{
	if (selectedObject != nullptr)
	{
		const auto* parent = selectedObject->GetParent();
		if (parent != nullptr)
		{
			const auto parentWorld = parent->GetWorldTransformMatrix();
			const auto local = matrix * DirectX::XMMatrixInverse(nullptr, parentWorld);
			selectedObject->GetTransform() = MakeTransformFromMatrix(local);
		}
		else
		{
			selectedObject->GetTransform() = MakeTransformFromMatrix(matrix);
		}
	}
}

void Scene::DrawHierarchyNode(GameObject& object) noexcept
{
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_DrawLinesToNodes;
	if (object.GetChildren().empty() && object.GetComponents().empty())
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
		for (auto& component : object.GetComponents())
		{
			if (component == nullptr)
			{
				continue;
			}
			ImGui::TreeNodeEx(
				reinterpret_cast<void*>(static_cast<uintptr_t>(component->GetId())),
				ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen,
				"Component (%llu)",
				static_cast<unsigned long long>(component->GetId())
			);
			if (ImGui::IsItemClicked())
			{
				selectedObject = &object;
			}
		}

		for (auto& child : object.GetChildren())
		{
			DrawHierarchyNode(*child);
		}
		ImGui::TreePop();
	}
}
