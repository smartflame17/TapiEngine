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
	Drawable* GetDrawable() noexcept;
	const Drawable* GetDrawable() const noexcept;
	DirectX::BoundingBox GetWorldBounds() const noexcept;

private:
	const char* GetInspectorTitle() const noexcept override;
	void DrawInspectorContents() noexcept override;

private:
	std::unique_ptr<Drawable> drawable;
};
