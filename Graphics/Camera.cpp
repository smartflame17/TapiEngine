#include "Camera.h"
#include "../imgui/imgui.h"


DirectX::XMMATRIX Camera::GetViewMatrix() const noexcept
{
	using namespace DirectX;

	// Load position
	XMVECTOR pos = XMVectorSet(x, y, z, 1.0f);

	// Build orientation
	XMMATRIX rot = XMMatrixRotationRollPitchYaw(pitch, yaw, roll);

	// Camera forward direction (LH: +Z)
	XMVECTOR forward = XMVector3TransformNormal(
		XMVectorSet(0.f, 0.f, 1.f, 0.f),
		rot
	);

	// Camera up direction
	XMVECTOR up = XMVector3TransformNormal(
		XMVectorSet(0.f, 1.f, 0.f, 0.f),
		rot
	);

	return XMMatrixLookToLH(pos, forward, up);
}

void Camera::SpawnControlWindow() noexcept
{
	if (ImGui::Begin("Camera")) 
	{
		ImGui::Text("Position");
		ImGui::SliderFloat("X", &x, -80.0f, 80.0f, "%.1f");
		ImGui::SliderFloat("Y", &y, -80.0f, 80.0f, "%.1f");
		ImGui::SliderFloat("Z", &z, -80.0f, 80.0f, "%.1f");

		ImGui::Text("Rotation");
		ImGui::SliderAngle("Roll", &roll, -180.0f, 180.0f);
		ImGui::SliderAngle("Pitch", &pitch, -180.0f, 180.0f);
		ImGui::SliderAngle("Yaw", &yaw, -180.0f, 180.0f);

		if (ImGui::Button("Reset")) 
		{
			Reset();
		}
	}
	ImGui::End();
}

void Camera::SetPosition(float x, float y, float z) noexcept
{
	this->x = x;
	this->y = y;
	this->z = z;
}

void Camera::SetRotation(float roll, float pitch, float yaw) noexcept
{
	this->roll = roll;
	this->pitch = pitch;
	this->yaw = yaw;
}

void Camera::Reset() noexcept
{
	x = 0.0f;
	y = 0.0f;
	z = 0.0f;
	roll = 0.0f;
	pitch = 0.0f;
	yaw = 0.0f;
}