#pragma once
#include "Graphics.h"

class Camera
{
public:
	DirectX::XMMATRIX GetViewMatrix() const noexcept;
	void SetPosition(float x, float y, float z) noexcept;
	void SetRotation(float pitch, float yaw, float roll) noexcept;
	void Reset() noexcept;

	void SpawnControlWindow() noexcept;	// imgui window for controlling camera

private:
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
	float pitch = 0.0f; // rotation around x-axis
	float yaw = 0.0f;   // rotation around y-axis
	float roll = 0.0f;  // rotation around z-axis
};