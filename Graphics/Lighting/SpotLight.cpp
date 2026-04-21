#include "SpotLight.h"

#include "../RenderQueue.h"
#include "../../Scene/GameObject.h"
#include <algorithm>
#include <cmath>

namespace
{
	DirectX::XMFLOAT3 Normalize(const DirectX::XMFLOAT3& input) noexcept
	{
		DirectX::XMFLOAT3 output;
		DirectX::XMStoreFloat3(&output, DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&input)));
		return output;
	}
}

SpotLight::SpotLight(Graphics& gfx, float radius)
	:
	Component(StaticType),
	gizmo(gfx, radius)
{
}

void SpotLight::SpawnControlWindow() noexcept
{
	if (ImGui::Begin("Spot Light"))
	{
		DrawInspectorContents();
	}
	ImGui::End();
}

const char* SpotLight::GetInspectorTitle() const noexcept
{
	return ComponentTypeToString(StaticType).data();
}

void SpotLight::DrawInspectorContents() noexcept
{
	if (auto* owner = TryGetGameObject())
	{
		auto position = owner->GetTransform().position;
		auto rotation = owner->GetTransform().rotation;
		bool transformChanged = false;

		ImGui::Text("Position");
		transformChanged |= ImGui::SliderFloat("X", &position.x, -60.0f, 60.0f, "%.1f");
		transformChanged |= ImGui::SliderFloat("Y", &position.y, -60.0f, 60.0f, "%.1f");
		transformChanged |= ImGui::SliderFloat("Z", &position.z, -60.0f, 60.0f, "%.1f");
		ImGui::Text("Rotation");
		transformChanged |= ImGui::SliderFloat("Pitch", &rotation.x, -DirectX::XM_PIDIV2, DirectX::XM_PIDIV2, "%.3f");
		transformChanged |= ImGui::SliderFloat("Yaw", &rotation.y, -DirectX::XM_PI, DirectX::XM_PI, "%.3f");
		transformChanged |= ImGui::SliderFloat("Roll", &rotation.z, -DirectX::XM_PI, DirectX::XM_PI, "%.3f");

		if (transformChanged)
		{
			owner->SetPosition(position.x, position.y, position.z);
			owner->SetRotation(rotation.x, rotation.y, rotation.z);
		}
	}

	ImGui::ColorEdit3("Color", &color.x);
	ImGui::SliderFloat("Intensity", &intensity, 0.0f, 8.0f, "%.2f");

	float innerDegrees = DirectX::XMConvertToDegrees(innerAngle);
	float outerDegrees = DirectX::XMConvertToDegrees(outerAngle);
	bool coneChanged = false;
	coneChanged |= ImGui::SliderFloat("Inner Cone", &innerDegrees, 1.0f, 80.0f, "%.1f deg");
	coneChanged |= ImGui::SliderFloat("Outer Cone", &outerDegrees, 1.0f, 89.0f, "%.1f deg");
	if (coneChanged)
	{
		SetConeAngles(DirectX::XMConvertToRadians(innerDegrees), DirectX::XMConvertToRadians(outerDegrees));
	}

	if (ImGui::Button("Reset"))
	{
		Reset();
	}
}

void SpotLight::Reset() noexcept
{
	color = { 1.0f, 0.9f, 0.7f };
	intensity = 1.2f;
	attConst = 1.0f;
	attLinear = 0.045f;
	attQuad = 0.0075f;
	innerAngle = 20.0f * DirectX::XM_PI / 180.0f;
	outerAngle = 30.0f * DirectX::XM_PI / 180.0f;

	if (auto* owner = TryGetGameObject())
	{
		owner->SetPosition(-3.0f, 5.0f, -3.0f);
		owner->SetRotation(0.9f, 0.75f, 0.0f);
	}
}

void SpotLight::Draw(Graphics& gfx) const noexcept(!IS_DEBUG)
{
	gizmo.SetPos(GetGameObject().GetTransform().position);
	gizmo.Draw(gfx);
}

RenderLight SpotLight::BuildRenderLight() const noexcept
{
	RenderLight light;
	light.type = LightType::Spot;
	light.color = color;
	light.intensity = intensity;
	light.attConst = attConst;
	light.attLinear = attLinear;
	light.attQuad = attQuad;

	if (const auto* owner = TryGetGameObject())
	{
		light.position = owner->GetTransform().position;
		const auto rotation = owner->GetTransform().rotation;
		const auto forward = DirectX::XMVector3TransformNormal(
			DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f),
			DirectX::XMMatrixRotationRollPitchYaw(rotation.x, rotation.y, rotation.z)
		);
		DirectX::XMStoreFloat3(&light.direction, forward);
		light.direction = Normalize(light.direction);
	}

	light.innerConeCos = std::cos(innerAngle);
	light.outerConeCos = std::cos(outerAngle);
	return light;
}

void SpotLight::SubmitGizmo(RenderQueueBuilder& builder) const
{
	builder.SubmitCallback(RenderPassId::EditorGizmos, [this](Graphics& gfx)
		{
			Draw(gfx);
		});
}

void SpotLight::SetColor(DirectX::XMFLOAT3 newColor) noexcept
{
	color = newColor;
}

void SpotLight::SetColor(float r, float g, float b) noexcept
{
	color = { r, g, b };
}

void SpotLight::SetIntensity(float newIntensity) noexcept
{
	intensity = newIntensity;
}

void SpotLight::SetAttenuation(float constant, float linear, float quadratic) noexcept
{
	attConst = constant;
	attLinear = linear;
	attQuad = quadratic;
}

void SpotLight::SetConeAngles(float innerAngleRadians, float outerAngleRadians) noexcept
{
	innerAngle = std::clamp(innerAngleRadians, 0.0174533f, DirectX::XM_PIDIV2 - 0.0174533f);
	outerAngle = std::clamp(outerAngleRadians, innerAngle, DirectX::XM_PIDIV2 - 0.0174533f);
}
