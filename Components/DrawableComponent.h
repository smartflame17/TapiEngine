#pragma once

#include "Component.h"
#include "../Graphics/Drawable/Drawable.h"
#include "../imgui/imgui.h"
#include <memory>

class DrawableComponent : public Component
{
public:
	explicit DrawableComponent(std::unique_ptr<Drawable> drawablePtr);

	void OnUpdate(float dt, bool isSimulationRunning) noexcept override;
	void OnRender(Graphics& gfx) const noexcept(!IS_DEBUG);
	void OnInspector() noexcept override;

private:
	Transform transform;
	std::unique_ptr<Drawable> drawable;
};
