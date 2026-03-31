#pragma once
#include "../Graphics.h"
#include "../IBindable/ConstantBuffers.h"
#include "../Drawable/SolidSphere.h"
#include "../../Components/Component.h"
#include "RenderLight.h"
#include "../../imgui/imgui.h"

class RenderQueueBuilder;

class PointLight : public Component
{
public:
	PointLight(Graphics& gfx, float radius = 0.5f);
	void SpawnControlWindow() noexcept;	// ImGui window for editing light properties
	void OnInspector() noexcept override;
	void Reset() noexcept;
	void Draw(Graphics& gfx) const noexcept(!IS_DEBUG);
	RenderLight BuildRenderLight() const noexcept;
	void SubmitGizmo(RenderQueueBuilder& builder) const;
	void SetColor(DirectX::XMFLOAT3 newColor) noexcept;
	void SetColor(float r, float g, float b) noexcept;
	void SetIntensity(float newIntensity) noexcept;
	void SetAttenuation(float constant, float linear, float quadratic) noexcept;

private:
	DirectX::XMFLOAT3 diffuseColor = { 1.0f, 1.0f, 1.0f };
	float diffuseIntensity = 1.0f;
	float attConst = 1.0f;
	float attLinear = 0.045f;
	float attQuad = 0.0075f;
	mutable SolidSphere mesh;
};
