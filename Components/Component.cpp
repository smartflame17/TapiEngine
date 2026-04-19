#include "Component.h"
#include "../Scene/GameObject.h"
#include "../Scene/Scene.h"
#include "../imgui/imgui.h"

std::uint64_t Component::nextId = 1;

Component::Component() :
	id(nextId++)
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

std::uint64_t Component::GetId() const noexcept
{
	return id;
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

	ImGui::PushID(reinterpret_cast<void*>(static_cast<uintptr_t>(id)));
	ImGui::BeginChild(
		"ComponentInspectorPanel",
		ImVec2(0.0f, 0.0f),
		ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding
	);

	if (ImGui::ArrowButton("##Collapse", inspectorCollapsed ? ImGuiDir_Right : ImGuiDir_Down))
	{
		inspectorCollapsed = !inspectorCollapsed;
	}

	ImGui::SameLine();
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(GetInspectorTitle());

	const float closeButtonWidth = ImGui::GetFrameHeight();
	const float closeButtonX = ImGui::GetWindowContentRegionMax().x - closeButtonWidth;
	if (closeButtonX > ImGui::GetCursorPosX())
	{
		ImGui::SameLine();
		ImGui::SetCursorPosX(closeButtonX);
	}

	if (ImGui::Button("X"))
	{
		pendingInspectorRemoval = true;
		ownerObject->GetScene().QueueComponentRemoval(*this);
	}

	if (!inspectorCollapsed && !pendingInspectorRemoval)
	{
		ImGui::Separator();
		DrawInspectorContents();
	}

	ImGui::EndChild();
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

const char* Component::GetInspectorTitle() const noexcept
{
	return "Component";
}

void Component::DrawInspectorContents() noexcept
{
}
