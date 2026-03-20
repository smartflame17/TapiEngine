#pragma once
#include "Graphics.h"
#include "../Components/Component.h"
#include "../imgui/imgui.h"

class Camera : public Component
{
public:
	Camera() = default;

	DirectX::XMMATRIX GetViewMatrix() const noexcept;
	void SetPosition(float x, float y, float z) noexcept;
	void SetRotation(float pitch, float yaw, float roll) noexcept;
	void Reset() noexcept;

	// helpers to move/rotate camera with mouse/keyboard
	void Translate(DirectX::XMFLOAT3 translation) noexcept;
	void Rotate(float dx, float dy) noexcept;

	void SpawnControlWindow() noexcept;	// imgui window for controlling camera
	void OnInspector() noexcept override;

private:
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
	float pitch = 0.0f; // rotation around x-axis
	float yaw = 0.0f;   // rotation around y-axis
	float roll = 0.0f;  // rotation around z-axis

public:
	float camSpeed = 1.0f;		// movement speed
	float rotateSpeed = 0.004f;	// mouse look sensitivity
};
