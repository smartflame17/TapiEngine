#include "DrawableComponent.h"
#include "../../Scene/GameObject.h"
#include "../Graphics/RenderQueue.h"

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

void DrawableComponent::Submit(RenderQueueBuilder& queueBuilder, const RenderView& view) const noexcept
{
	if (drawable != nullptr)
	{
		drawable->SetExternalTransformMatrix(GetGameObject().GetWorldTransformMatrix());
		// all drawables submitted through this component are considered opaque for sorting purposes, as they are expected to be static meshes..
		queueBuilder.SubmitOpaque(*drawable, GetGameObject().GetWorldTransformMatrix());
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
