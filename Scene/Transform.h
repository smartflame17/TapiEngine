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
	DirectX::XMFLOAT4X4 m;
	DirectX::XMStoreFloat4x4(&m, matrix);

	transform.position = { m._41, m._42, m._43 };

	const auto length3 = [](float x, float y, float z) noexcept
	{
		return std::sqrt(x * x + y * y + z * z);
	};

	transform.scale.x = length3(m._11, m._12, m._13);
	transform.scale.y = length3(m._21, m._22, m._23);
	transform.scale.z = length3(m._31, m._32, m._33);

	const float safeScaleX = (transform.scale.x > 0.0f) ? transform.scale.x : 1.0f;
	const float safeScaleY = (transform.scale.y > 0.0f) ? transform.scale.y : 1.0f;
	const float safeScaleZ = (transform.scale.z > 0.0f) ? transform.scale.z : 1.0f;

	const float n11 = m._11 / safeScaleX;
	const float n12 = m._12 / safeScaleX;
	const float n13 = m._13 / safeScaleX;
	const float n23 = m._23 / safeScaleY;
	const float n33 = m._33 / safeScaleZ;

	transform.rotation.x = std::atan2(n23, n33);
	transform.rotation.y = std::atan2(-n13, std::sqrt(n23 * n23 + n33 * n33));
	transform.rotation.z = std::atan2(n12, n11);

	return transform;
}
