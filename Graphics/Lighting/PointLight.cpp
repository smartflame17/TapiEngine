#include "PointLight.h"
#include "../RenderQueue.h"
#include "../../Scene/GameObject.h"

PointLight::PointLight(Graphics& gfx, float radius)
	:
	mesh(gfx, radius)
{
}

void PointLight::SpawnControlWindow() noexcept
{
	if (ImGui::Begin("Light"))
	{
		OnInspector();
	}
	ImGui::End();
}

void PointLight::OnInspector() noexcept
{
	ImGui::Text("Point Light");
	ImGui::Separator();
	if (auto* owner = TryGetGameObject())
	{
		auto position = owner->GetTransform().position;
		ImGui::Text("Position");
		bool changed = false;
		changed |= ImGui::SliderFloat("X", &position.x, -60.0f, 60.0f, "%.1f");
		changed |= ImGui::SliderFloat("Y", &position.y, -60.0f, 60.0f, "%.1f");
		changed |= ImGui::SliderFloat("Z", &position.z, -60.0f, 60.0f, "%.1f");
		if (changed)
		{
			owner->SetPosition(position.x, position.y, position.z);
		}
	}
	ImGui::ColorEdit3("Color", &diffuseColor.x);
	ImGui::SliderFloat("Intensity", &diffuseIntensity, 0.0f, 8.0f, "%.2f");
	if (ImGui::Button("Reset"))
	{
		Reset();
	}
}

void PointLight::Reset() noexcept
{
	diffuseColor = { 1.0f, 1.0f, 1.0f };
	diffuseIntensity = 1.0f;
	if (auto* owner = TryGetGameObject())
	{
		owner->SetPosition(0.0f, 4.0f, -2.0f);
	}
}

void PointLight::Draw(Graphics& gfx) const noexcept(!IS_DEBUG)
{
	mesh.SetPos(GetGameObject().GetTransform().position);
	mesh.Draw(gfx);
}

RenderLight PointLight::BuildRenderLight() const noexcept
{
	RenderLight light;
	light.type = LightType::Point;
	light.color = diffuseColor;
	light.intensity = diffuseIntensity;
	if (const auto* owner = TryGetGameObject())
	{
		light.position = owner->GetTransform().position;
	}
	light.attConst = attConst;
	light.attLinear = attLinear;
	light.attQuad = attQuad;
	return light;
}

void PointLight::SubmitGizmo(RenderQueueBuilder& builder) const
{
	builder.SubmitCallback(RenderPassId::EditorGizmos, [this](Graphics& gfx)
		{
			Draw(gfx);
		});
}

void PointLight::SetColor(DirectX::XMFLOAT3 newColor) noexcept
{
	diffuseColor = newColor;
}

void PointLight::SetColor(float r, float g, float b) noexcept
{
	diffuseColor = { r, g, b };
}

void PointLight::SetIntensity(float newIntensity) noexcept
{
	diffuseIntensity = newIntensity;
}

void PointLight::SetAttenuation(float constant, float linear, float quadratic) noexcept
{
	attConst = constant;
	attLinear = linear;
	attQuad = quadratic;
}
