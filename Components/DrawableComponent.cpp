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
		drawable->Draw(gfx);
	}
}

void DrawableComponent::OnInspector() noexcept
{
	if (auto component = dynamic_cast<Component*>(drawable.get()))
	{
		component->OnInspector();
	}
	else Component::OnInspector();
}