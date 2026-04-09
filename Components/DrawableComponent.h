#pragma once

#include "Component.h"
#include "../Graphics/Drawable/Drawable.h"
#include <DirectXCollision.h>
#include "../imgui/imgui.h"
#include <memory>

class RenderQueueBuilder;
struct RenderView;

class DrawableComponent : public Component
{
public:
	explicit DrawableComponent(std::unique_ptr<Drawable> drawablePtr);

	void OnUpdate(float dt, bool isSimulationRunning) noexcept override;
	void Submit(RenderQueueBuilder& queueBuilder, const RenderView& view) const noexcept;
	void OnInspector() noexcept override;
	Drawable* GetDrawable() noexcept;
	const Drawable* GetDrawable() const noexcept;
	DirectX::BoundingBox GetWorldBounds() const noexcept;

private:
	std::unique_ptr<Drawable> drawable;
};
