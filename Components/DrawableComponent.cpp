#include "DrawableComponent.h"
#include "../../Scene/GameObject.h"

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
		drawable->SetExternalTransformMatrix(GetGameObject().GetWorldTransformMatrix());
		drawable->Draw(gfx);
	}
}

void DrawableComponent::OnInspector() noexcept
{
	if (ImGui::TreeNodeEx("DrawableComponent", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (drawable)
		{
			drawable->DrawInspector();
		}
		ImGui::TreePop();
	}
}

Drawable* DrawableComponent::GetDrawable() noexcept
{
	return drawable.get();
}

const Drawable* DrawableComponent::GetDrawable() const noexcept
{
	return drawable.get();
}

DirectX::BoundingBox DrawableComponent::GetWorldBounds() const noexcept
{
	if (drawable == nullptr)
	{
		return {};
	}

	return drawable->GetWorldBounds(GetGameObject().GetWorldTransformMatrix());
}
