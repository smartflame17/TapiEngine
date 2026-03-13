#include "Camera.h"

#define PI 3.14159265359f
#define MAX_DISTANCE 1000.0f		// max distance camera can move in any direction

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

void Camera::Translate(DirectX::XMFLOAT3 translation) noexcept
{
	using namespace DirectX;
	// Build orientation
	XMMATRIX rot = XMMatrixRotationRollPitchYaw(pitch, yaw, roll);
	// Transform translation vector by camera rotation
	XMVECTOR transVec = XMLoadFloat3(&translation);
	XMVECTOR worldTransVec = XMVector3TransformNormal(transVec, rot);

	XMFLOAT3 worldTrans;
	XMStoreFloat3(&worldTrans, worldTransVec);

	x += worldTrans.x;
	y += worldTrans.y;
	z += worldTrans.z;

	// Clamp position to max distance
	x = std::max(-MAX_DISTANCE, std::min(MAX_DISTANCE, x));
	y = std::max(-MAX_DISTANCE, std::min(MAX_DISTANCE, y));
	z = std::max(-MAX_DISTANCE, std::min(MAX_DISTANCE, z));
}

void Camera::Rotate(float dx, float dy) noexcept
{
	yaw += dx;
	pitch += dy;

	// Clamp pitch to approx 90 degrees (just under PI/2)
	constexpr float limit = PI / 2.0f - 0.01f;
	pitch = std::max(-limit, std::min(limit, pitch));
}

void Camera::SpawnControlWindow() noexcept
{
	if (ImGui::Begin("Camera"))
	{
		OnInspector();
	}
	ImGui::End();
}

void Camera::OnInspector() noexcept
{
	ImGui::Text("Camera");
	ImGui::Separator();
	ImGui::Text("Position");
	ImGui::SliderFloat("X", &x, -MAX_DISTANCE, MAX_DISTANCE, "%.1f");
	ImGui::SliderFloat("Y", &y, -MAX_DISTANCE, MAX_DISTANCE, "%.1f");
	ImGui::SliderFloat("Z", &z, -MAX_DISTANCE, MAX_DISTANCE, "%.1f");

	if (ImGui::Button("Reset"))
	{
		Reset();
	}
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
