#pragma once

#include "../Graphics.h"
#include "../Drawable/SolidSphere.h"
#include "../../Components/Component.h"
#include "RenderLight.h"
#include "../../imgui/imgui.h"

class RenderQueueBuilder;

class DirectionalLight : public Component
{
public:
	DirectionalLight(Graphics& gfx, float radius = 0.35f);
	void SpawnControlWindow() noexcept;
	void OnInspector() noexcept override;
	void Reset() noexcept;
	RenderLight BuildRenderLight() const noexcept;
	void SubmitGizmo(RenderQueueBuilder& builder) const;

private:
	mutable SolidSphere gizmo;
	DirectX::XMFLOAT3 color = { 1.0f, 0.97f, 0.75f };
	float intensity = 1.0f;
};
