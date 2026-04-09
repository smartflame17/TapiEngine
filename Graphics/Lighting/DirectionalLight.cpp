#include "DirectionalLight.h"
#include "../RenderQueue.h"
#include "../../Scene/GameObject.h"
#include <algorithm>
#include <array>
#include <limits>

namespace
{
	DirectX::XMFLOAT3 Normalize(const DirectX::XMFLOAT3& input) noexcept
	{
		DirectX::XMFLOAT3 output;
		DirectX::XMStoreFloat3(&output, DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&input)));
		return output;
	}

	DirectX::XMVECTOR SelectLightUpVector(DirectX::FXMVECTOR lightDirection) noexcept
	{
		DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
		const float alignment = std::abs(DirectX::XMVectorGetX(DirectX::XMVector3Dot(lightDirection, up)));
		if (alignment > 0.99f)
		{
			up = DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
		}
		return up;
	}
}

DirectionalLight::DirectionalLight(Graphics& gfx)
	/*:
	gizmo(gfx, radius)*/
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
	light.pDirectionalLight = this;

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

DirectX::XMMATRIX DirectionalLight::GetLightViewProjection(const DirectX::BoundingFrustum& visibleFrustum) const noexcept
{
	const auto light = BuildRenderLight();
	const auto direction = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&light.direction));

	std::array<DirectX::XMFLOAT3, DirectX::BoundingFrustum::CORNER_COUNT> corners = {};
	visibleFrustum.GetCorners(corners.data());

	DirectX::XMVECTOR center = DirectX::XMVectorZero();
	for (const auto& corner : corners)
	{
		center = DirectX::XMVectorAdd(center, DirectX::XMLoadFloat3(&corner));
	}
	center = DirectX::XMVectorScale(center, 1.0f / static_cast<float>(corners.size()));

	float radius = 0.0f;
	for (const auto& corner : corners)
	{
		const auto offset = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&corner), center);
		radius = std::max(radius, DirectX::XMVectorGetX(DirectX::XMVector3Length(offset)));
	}

	const float depthPadding = std::max(radius * 0.1f, 1.0f);
	const auto eye = DirectX::XMVectorSubtract(center, DirectX::XMVectorScale(direction, radius + depthPadding));
	const auto view = DirectX::XMMatrixLookToLH(eye, direction, SelectLightUpVector(direction));

	DirectX::XMFLOAT3 minBounds = {
		std::numeric_limits<float>::max(),
		std::numeric_limits<float>::max(),
		std::numeric_limits<float>::max()
	};
	DirectX::XMFLOAT3 maxBounds = {
		-std::numeric_limits<float>::max(),
		-std::numeric_limits<float>::max(),
		-std::numeric_limits<float>::max()
	};

	for (const auto& corner : corners)
	{
		DirectX::XMFLOAT3 lightSpaceCorner;
		DirectX::XMStoreFloat3(&lightSpaceCorner, DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&corner), view));
		minBounds.x = std::min(minBounds.x, lightSpaceCorner.x);
		minBounds.y = std::min(minBounds.y, lightSpaceCorner.y);
		minBounds.z = std::min(minBounds.z, lightSpaceCorner.z);
		maxBounds.x = std::max(maxBounds.x, lightSpaceCorner.x);
		maxBounds.y = std::max(maxBounds.y, lightSpaceCorner.y);
		maxBounds.z = std::max(maxBounds.z, lightSpaceCorner.z);
	}

	const float xyPadding = std::max(radius * 0.05f, 0.5f);
	const float nearPlane = std::max(0.1f, minBounds.z - depthPadding);
	const float farPlane = std::max(nearPlane + 1.0f, maxBounds.z + depthPadding);
	const auto projection = DirectX::XMMatrixOrthographicOffCenterLH(
		minBounds.x - xyPadding,
		maxBounds.x + xyPadding,
		minBounds.y - xyPadding,
		maxBounds.y + xyPadding,
		nearPlane,
		farPlane
	);

	return view * projection;
}

//void DirectionalLight::SubmitGizmo(RenderQueueBuilder& builder) const
//{
//	builder.SubmitCallback(RenderPassId::EditorGizmos, [this](Graphics& gfx)
//		{
//			if (const auto* owner = TryGetGameObject())
//			{
//				const auto origin = owner->GetTransform().position;
//				const auto light = BuildRenderLight();
//				const DirectX::XMFLOAT3 offset = {
//					origin.x - light.direction.x,
//					origin.y - light.direction.y,
//					origin.z - light.direction.z
//				};
//				gizmo.SetPos(offset);
//				gizmo.Draw(gfx);
//			}
//		});
//}

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
