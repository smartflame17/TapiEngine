#pragma once

#include <DirectXMath.h>
#include <cmath>

struct Transform
{
	DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 rotation = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 scale = { 1.0f, 1.0f, 1.0f };
};

inline DirectX::XMMATRIX MakeTransformMatrix(const Transform& transform) noexcept
{
	return
		DirectX::XMMatrixScaling(transform.scale.x, transform.scale.y, transform.scale.z) *
		DirectX::XMMatrixRotationRollPitchYaw(transform.rotation.x, transform.rotation.y, transform.rotation.z) *
		DirectX::XMMatrixTranslation(transform.position.x, transform.position.y, transform.position.z);
}

inline Transform MakeTransformFromMatrix(DirectX::FXMMATRIX matrix) noexcept
{
	Transform transform;
	DirectX::XMVECTOR scale;
	DirectX::XMVECTOR rotation;
	DirectX::XMVECTOR translation;

	if (DirectX::XMMatrixDecompose(&scale, &rotation, &translation, matrix))
	{
		DirectX::XMStoreFloat3(&transform.position, translation);
		DirectX::XMStoreFloat3(&transform.scale, scale);

		DirectX::XMFLOAT4 quat;
		DirectX::XMStoreFloat4(&quat, rotation);
		const float x = quat.x;
		const float y = quat.y;
		const float z = quat.z;
		const float w = quat.w;

		const float sinr_cosp = 2.0f * (w * x + y * z);
		const float cosr_cosp = 1.0f - 2.0f * (x * x + y * y);
		transform.rotation.x = std::atan2(sinr_cosp, cosr_cosp);

		const float sinp = 2.0f * (w * y - z * x);
		if (std::abs(sinp) >= 1.0f)
			transform.rotation.y = std::copysign(DirectX::XM_PIDIV2, sinp);
		else
			transform.rotation.y = std::asin(sinp);

		const float siny_cosp = 2.0f * (w * z + x * y);
		const float cosy_cosp = 1.0f - 2.0f * (y * y + z * z);
		transform.rotation.z = std::atan2(siny_cosp, cosy_cosp);
	}

	return transform;
}
