#pragma once
#include "Graphics.h"
#include <DirectXCollision.h>
#include <algorithm>
#include "../Components/Component.h"
#include "../Scene/GameObject.h"
#include "../imgui/imgui.h"

class Camera : public Component
{
public:
	static constexpr ComponentType StaticType = ComponentType::Camera;
	Camera() noexcept;

	DirectX::XMMATRIX GetViewMatrix() const noexcept;
	const DirectX::BoundingFrustum& GetFrustum() const noexcept;
	void UpdateFrustum(DirectX::FXMMATRIX projection) noexcept;
	void SetPosition(float x, float y, float z) noexcept;
	void SetRotation(float pitch, float yaw, float roll) noexcept;
	void Reset() noexcept;

	// helpers to move/rotate camera with mouse/keyboard
	void Translate(DirectX::XMFLOAT3 translation) noexcept;
	void Rotate(float dx, float dy) noexcept;

	void SpawnControlWindow() noexcept;	// imgui window for controlling camera
private:
	const char* GetInspectorTitle() const noexcept override;
	void DrawInspectorContents() noexcept override;

private:
	DirectX::XMFLOAT3 GetPosition() const noexcept;
	DirectX::XMFLOAT3 GetRotation() const noexcept;
private:
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
	float pitch = 0.0f; // rotation around x-axis
	float yaw = 0.0f;   // rotation around y-axis
	float roll = 0.0f;  // rotation around z-axis
	DirectX::BoundingFrustum frustum;

public:
	float camSpeed = 0.1f;		// movement speed
	float rotateSpeed = 0.004f;	// mouse look sensitivity
};
