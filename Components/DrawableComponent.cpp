#include "DrawableComponent.h"

DrawableComponent::DrawableComponent(std::unique_ptr<Drawable> drawablePtr) :
	drawable(std::move(drawablePtr))
{}

void DrawableComponent::OnUpdate(float dt, bool isSimulationRunning) noexcept
{
	if (isSimulationRunning && drawable != nullptr)
	{
		drawable->Update(dt);
	}
}

void DrawableComponent::OnRender(Graphics& gfx) const noexcept(!IS_DEBUG)
{
	if (drawable != nullptr)
	{
		drawable->SetTransform(GetTransform());
		drawable->SetExternalTransformMatrix(GetGameObject().GetWorldTransformMatrix());
		drawable->Draw(gfx);
	}
}

void DrawableComponent::OnInspector() noexcept
{
	if (ImGui::TreeNodeEx("DrawableComponent", ImGuiTreeNodeFlags_DefaultOpen))
	{
		auto& transform = GetTransform();
		ImGui::Text("Transform");
		ImGui::DragFloat3("Position", &transform.position.x, 0.05f);
		ImGui::DragFloat3("Rotation", &transform.rotation.x, 0.01f);
		ImGui::DragFloat3("Scale", &transform.scale.x, 0.05f, 0.01f, 200.0f, "%.2f");
		if (drawable)
		{
			drawable->DrawInspector();
		}
		ImGui::TreePop();
	}
}
