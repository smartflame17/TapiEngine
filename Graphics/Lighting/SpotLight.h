#pragma once

#include "../Graphics.h"
#include "../Drawable/SolidSphere.h"
#include "../../Components/Component.h"
#include "RenderLight.h"
#include "../../imgui/imgui.h"

class RenderQueueBuilder;

class SpotLight : public Component
{
public:
	SpotLight(Graphics& gfx, float radius = 0.4f);
	void SpawnControlWindow() noexcept;
	void Reset() noexcept;
	void Draw(Graphics& gfx) const noexcept(!IS_DEBUG);
	RenderLight BuildRenderLight() const noexcept;
	void SubmitGizmo(RenderQueueBuilder& builder) const;
	void SetColor(DirectX::XMFLOAT3 newColor) noexcept;
	void SetColor(float r, float g, float b) noexcept;
	void SetIntensity(float newIntensity) noexcept;
	void SetAttenuation(float constant, float linear, float quadratic) noexcept;
	void SetConeAngles(float innerAngleRadians, float outerAngleRadians) noexcept;

private:
	const char* GetInspectorTitle() const noexcept override;
	void DrawInspectorContents() noexcept override;

private:
	DirectX::XMFLOAT3 color = { 1.0f, 0.9f, 0.7f };
	float intensity = 1.2f;
	float attConst = 1.0f;
	float attLinear = 0.045f;
	float attQuad = 0.0075f;
	float innerAngle = 20.0f * DirectX::XM_PI / 180.0f;
	float outerAngle = 30.0f * DirectX::XM_PI / 180.0f;
	mutable SolidSphere gizmo;
};
