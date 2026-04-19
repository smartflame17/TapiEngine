#include "Camera.h"

#define PI 3.14159265359f
#define MAX_DISTANCE 1000.0f		// max distance camera can move in any direction

DirectX::XMFLOAT3 Camera::GetPosition() const noexcept
{
	if (const GameObject* owner = TryGetGameObject())
	{
		return owner->GetTransform().position;
	}
	return { x, y, z };
}

DirectX::XMFLOAT3 Camera::GetRotation() const noexcept
{
	if (const GameObject* owner = TryGetGameObject())
	{
		return owner->GetTransform().rotation;
	}
	return { pitch, yaw, roll };
}

DirectX::XMMATRIX Camera::GetViewMatrix() const noexcept
{
	using namespace DirectX;
	const XMFLOAT3 position = GetPosition();
	const XMFLOAT3 rotation = GetRotation();

	// Load position
	XMVECTOR pos = XMVectorSet(position.x, position.y, position.z, 1.0f);

	// Build orientation
	XMMATRIX rot = XMMatrixRotationRollPitchYaw(rotation.x, rotation.y, rotation.z);

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

const DirectX::BoundingFrustum& Camera::GetFrustum() const noexcept
{
	return frustum;
}

void Camera::UpdateFrustum(DirectX::FXMMATRIX projection) noexcept
{
	DirectX::BoundingFrustum viewSpaceFrustum;
	DirectX::BoundingFrustum::CreateFromMatrix(viewSpaceFrustum, projection);
	viewSpaceFrustum.Transform(frustum, DirectX::XMMatrixInverse(nullptr, GetViewMatrix()));
}

void Camera::Translate(DirectX::XMFLOAT3 translation) noexcept
{
	using namespace DirectX;
	const XMFLOAT3 rotation = GetRotation();
	XMFLOAT3 position = GetPosition();

	// Build orientation
	XMMATRIX rot = XMMatrixRotationRollPitchYaw(rotation.x, rotation.y, rotation.z);
	// Transform translation vector by camera rotation
	XMVECTOR transVec = XMLoadFloat3(&translation);
	XMVECTOR worldTransVec = XMVector3TransformNormal(transVec, rot);

	XMFLOAT3 worldTrans;
	XMStoreFloat3(&worldTrans, worldTransVec);

	position.x += worldTrans.x;
	position.y += worldTrans.y;
	position.z += worldTrans.z;

	// Clamp position to max distance
	position.x = std::max(-MAX_DISTANCE, std::min(MAX_DISTANCE, position.x));
	position.y = std::max(-MAX_DISTANCE, std::min(MAX_DISTANCE, position.y));
	position.z = std::max(-MAX_DISTANCE, std::min(MAX_DISTANCE, position.z));

	SetPosition(position.x, position.y, position.z);
}

void Camera::Rotate(float dx, float dy) noexcept
{
	DirectX::XMFLOAT3 rotation = GetRotation();
	rotation.y += dx;
	rotation.x += dy;

	// Clamp pitch to approx 90 degrees (just under PI/2)
	constexpr float limit = PI / 2.0f - 0.01f;
	//pitch = std::max(-limit, std::min(limit, pitch));
	rotation.x = std::max(-limit, std::min(limit, rotation.x));

	SetRotation(rotation.x, rotation.y, rotation.z);
}

void Camera::SpawnControlWindow() noexcept
{
	if (ImGui::Begin("Camera"))
	{
		DrawInspectorContents();
	}
	ImGui::End();
}

const char* Camera::GetInspectorTitle() const noexcept
{
	return "Camera";
}

void Camera::DrawInspectorContents() noexcept
{
	DirectX::XMFLOAT3 position = GetPosition();
	DirectX::XMFLOAT3 rotation = GetRotation();

	ImGui::Text("Position");
	/*ImGui::SliderFloat("X", &x, -MAX_DISTANCE, MAX_DISTANCE, "%.1f");
	ImGui::SliderFloat("Y", &y, -MAX_DISTANCE, MAX_DISTANCE, "%.1f");
	ImGui::SliderFloat("Z", &z, -MAX_DISTANCE, MAX_DISTANCE, "%.1f");*/
	bool positionChanged = false;
	positionChanged |= ImGui::SliderFloat("X", &position.x, -MAX_DISTANCE, MAX_DISTANCE, "%.1f");
	positionChanged |= ImGui::SliderFloat("Y", &position.y, -MAX_DISTANCE, MAX_DISTANCE, "%.1f");
	positionChanged |= ImGui::SliderFloat("Z", &position.z, -MAX_DISTANCE, MAX_DISTANCE, "%.1f");
	if (positionChanged)
	{
		SetPosition(position.x, position.y, position.z);
	}
	bool rotationChanged = false;
	rotationChanged |= ImGui::SliderFloat("Pitch", &rotation.x, -PI / 2.0f + 0.01f, PI / 2.0f - 0.01f, "%.3f");
	rotationChanged |= ImGui::SliderFloat("Yaw", &rotation.y, -PI, PI, "%.3f");
	rotationChanged |= ImGui::SliderFloat("Roll", &rotation.z, -PI, PI, "%.3f");
	if (rotationChanged)
	{
		SetRotation(rotation.x, rotation.y, rotation.z);
	}

	ImGui::SliderFloat("Speed", &camSpeed, 0.1f, 10.0f, "%.1f");
	ImGui::SliderFloat("Sensitivity", &rotateSpeed, 0.001f, 0.01f, "%.3f");

	if (ImGui::Button("Reset"))
	{
		Reset();
	}
}

void Camera::SetPosition(float x, float y, float z) noexcept
{
	if (GameObject* owner = TryGetGameObject())
	{
		owner->SetPosition(x, y, z);
	}
	this->x = x;
	this->y = y;
	this->z = z;
}

void Camera::SetRotation(float pitch, float yaw, float roll) noexcept
{
	if (GameObject* owner = TryGetGameObject())
	{
		owner->SetRotation(pitch, yaw, roll);
	}
	this->pitch = pitch;
	this->yaw = yaw;
	this->roll = roll;
}

void Camera::Reset() noexcept
{
	x = 0.0f;
	y = 0.0f;
	z = 0.0f;
	roll = 0.0f;
	pitch = 0.0f;
	yaw = 0.0f;
	SetPosition(0.0f, 0.0f, 0.0f);
	SetRotation(0.0f, 0.0f, 0.0f);
}
