#include "Component.h"
#include "../Scene/GameObject.h"
#include "../Scene/Scene.h"
#include "../imgui/imgui.h"
#include <stdexcept>

Component::Component(ComponentType type):
	type(type)
{}

void Component::SetOwner(GameObject* gameObject) noexcept
{
	owner = gameObject;
}

GameObject& Component::GetGameObject() const noexcept
{
	assert(owner != nullptr);
	return *owner;
}

GameObject* Component::TryGetGameObject() const noexcept
{
	return owner;
}

const Guid& Component::GetId() const noexcept
{
	return id;
}

void Component::SetId(const Guid& savedId)
{
	if (savedId.IsNull()) throw std::invalid_argument("Component identity cannot be a null UUID.");
	id = savedId;
}

void Component::OnUpdate(float dt, bool isSimulationRunning) noexcept
{
}

void Component::OnInspector() noexcept
{
	if (pendingInspectorRemoval)
	{
		return;
	}

	GameObject* ownerObject = TryGetGameObject();
	if (ownerObject == nullptr)
	{
		return;
	}

	// Keep the header and its contents scoped to this component's lifetime ID.
	ImGui::PushID(id.ToString().c_str());
	bool visible = true;
	const bool expanded = ImGui::CollapsingHeader(GetInspectorTitle(), &visible, ImGuiTreeNodeFlags_DefaultOpen);
	if (!visible)
	{
		ownerObject->GetScene().QueueComponentRemoval(*this);
	}
	else if (expanded)
	{
		DrawInspectorContents();
	}

	ImGui::PopID();
}

bool Component::IsPendingInspectorRemoval() const noexcept
{
	return pendingInspectorRemoval;
}

void Component::MarkPendingInspectorRemoval(bool pending) noexcept
{
	pendingInspectorRemoval = pending;
}

// This function should be overridden by components that want to have a custom title in the inspector
const char* Component::GetInspectorTitle() const noexcept
{
	return "Component";
}

void Component::DrawInspectorContents() noexcept
{
}
