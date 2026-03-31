#include "DirectionalLight.h"
#include "../RenderQueue.h"
#include "../../Scene/GameObject.h"

namespace
{
	DirectX::XMFLOAT3 Normalize(const DirectX::XMFLOAT3& input) noexcept
	{
		DirectX::XMFLOAT3 output;
		DirectX::XMStoreFloat3(&output, DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&input)));
		return output;
	}
}

DirectionalLight::DirectionalLight(Graphics& gfx, float radius)
	:
	gizmo(gfx, radius)
{
}

void DirectionalLight::SpawnControlWindow() noexcept
{
	if (ImGui::Begin("Directional Light"))
	{
		OnInspector();
	}
	ImGui::End();
}

void DirectionalLight::OnInspector() noexcept
{
	ImGui::Text("Directional Light");
	ImGui::Separator();
	if (auto* owner = TryGetGameObject())
	{
		auto rotation = owner->GetTransform().rotation;
		bool changed = false;
		changed |= ImGui::SliderFloat("Pitch", &rotation.x, -DirectX::XM_PIDIV2, DirectX::XM_PIDIV2, "%.3f");
		changed |= ImGui::SliderFloat("Yaw", &rotation.y, -DirectX::XM_PI, DirectX::XM_PI, "%.3f");
		changed |= ImGui::SliderFloat("Roll", &rotation.z, -DirectX::XM_PI, DirectX::XM_PI, "%.3f");
		if (changed)
		{
			owner->SetRotation(rotation.x, rotation.y, rotation.z);
		}
	}
	ImGui::ColorEdit3("Color", &color.x);
	ImGui::SliderFloat("Intensity", &intensity, 0.0f, 8.0f, "%.2f");
	if (ImGui::Button("Reset"))
	{
		Reset();
	}
}

void DirectionalLight::Reset() noexcept
{
	color = { 1.0f, 0.97f, 0.75f };
	intensity = 1.0f;
	if (auto* owner = TryGetGameObject())
	{
		owner->SetRotation(0.4f, -0.7f, 0.0f);
	}
}

RenderLight DirectionalLight::BuildRenderLight() const noexcept
{
	RenderLight light;
	light.type = LightType::Directional;
	light.color = color;
	light.intensity = intensity;

	if (const auto* owner = TryGetGameObject())
	{
		const auto rotation = owner->GetTransform().rotation;
		const auto forward = DirectX::XMVector3TransformNormal(
			DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f),
			DirectX::XMMatrixRotationRollPitchYaw(rotation.x, rotation.y, rotation.z)
		);
		DirectX::XMStoreFloat3(&light.direction, forward);
		light.direction = Normalize(light.direction);
		light.position = owner->GetTransform().position;
	}

	return light;
}

void DirectionalLight::SubmitGizmo(RenderQueueBuilder& builder) const
{
	builder.SubmitCallback(RenderPassId::EditorGizmos, [this](Graphics& gfx)
		{
			if (const auto* owner = TryGetGameObject())
			{
				const auto origin = owner->GetTransform().position;
				const auto light = BuildRenderLight();
				const DirectX::XMFLOAT3 offset = {
					origin.x - light.direction.x,
					origin.y - light.direction.y,
					origin.z - light.direction.z
				};
				gizmo.SetPos(offset);
				gizmo.Draw(gfx);
			}
		});
}

void DirectionalLight::SetColor(DirectX::XMFLOAT3 newColor) noexcept
{
	color = newColor;
}

void DirectionalLight::SetColor(float r, float g, float b) noexcept
{
	color = { r, g, b };
}

void DirectionalLight::SetIntensity(float newIntensity) noexcept
{
	intensity = newIntensity;
}
