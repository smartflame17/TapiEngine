#pragma once
#include "Graphics.h"
#include "../Components/Component.h"
#include "../imgui/imgui.h"
#include <algorithm>

class Camera : public Component
{
public:
	DirectX::XMMATRIX GetViewMatrix() const noexcept;
	void SetPosition(float x, float y, float z) noexcept;
	void SetRotation(float pitch, float yaw, float roll) noexcept;
	void Reset() noexcept;

	void Translate(DirectX::XMFLOAT3 translation) noexcept;
	void Rotate(float dx, float dy) noexcept;

	void SpawnControlWindow() noexcept;

private:
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
	float pitch = 0.0f;
	float yaw = 0.0f;
	float roll = 0.0f;
};
