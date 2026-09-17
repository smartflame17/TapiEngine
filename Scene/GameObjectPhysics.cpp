#include "GameObject.h"
#include "../Components/Rigidbody.h"
#include "../Components/Collider.h"
#include "../Physics/Physics.h"
#include "../Physics/PhysicsMath.h"
#include <box3d/box3d.h>
#include <cmath>

using namespace DirectX;

void Scene::SynchronizePhysicsTransforms() noexcept
{
	// Snapshot all poses before changing any parent. Static and sleeping bodies participate.
	const auto capture = [&](auto&& self, GameObject& object) -> void
	{
		if (object.IsPendingKill()) return;
		object.CapturePhysicsPose();
		for (auto& child : object.children) self(self, *child);
	};
	const auto apply = [&](auto&& self, GameObject& object) -> void
	{
		if (object.IsPendingKill()) return;
		object.ApplyPhysicsPose();
		for (auto& child : object.children) self(self, *child);
	};
	for (auto& root : rootObjects) capture(capture, *root);
	for (auto& root : rootObjects) apply(apply, *root);
}

void GameObject::InitializePhysicsComponent(Component& component)
{
	if (component.IsType(ComponentType::Rigidbody) || component.IsType(ComponentType::Collider))
	{
		Physics::GetInstance(); // Validate lifetime before changing existing resources.
		RefreshPhysics();
	}
}

void GameObject::DetachPhysicsComponent(Component& component) noexcept
{
	if (component.IsType(ComponentType::Collider))
	{
		auto& collider = static_cast<Collider&>(component);
		collider.ReleaseShape();
		collider.ReleaseStandaloneBody();
	}
	else if (component.IsType(ComponentType::Rigidbody))
	{
		if (auto* collider = GetComponent<Collider>()) collider->ReleaseShape();
		static_cast<Rigidbody&>(component).ReleaseBody();
	}
	else return;
	RefreshPhysics();
}

void GameObject::ReleasePhysics() noexcept
{
	// Include pending removals. No replacement bodies are created during object teardown.
	for (auto& component : components)
		if (component->IsType(ComponentType::Collider))
		{
			auto& collider = static_cast<Collider&>(*component);
			collider.ReleaseShape();
			collider.ReleaseStandaloneBody();
		}
	for (auto& component : components)
		if (component->IsType(ComponentType::Rigidbody)) static_cast<Rigidbody&>(*component).ReleaseBody();
	hasPhysicsPose = false;
}

void GameObject::RecreatePhysicsBody() noexcept
{
	if (isPendingKill) return;
	CapturePhysicsPose();
	ApplyPhysicsPose();
	if (auto* collider = GetComponent<Collider>()) collider->ReleaseShape();
	if (auto* body = GetComponent<Rigidbody>()) body->ReleaseBody();
	RefreshPhysics();
}

bool GameObject::GetPhysicsWorldPose(XMFLOAT3& position, XMFLOAT4& rotation, XMFLOAT3& scale) const noexcept
{
	for (auto* object = this; object; object = object->parent)
	{
		const auto& t = object->transform;
		if (!PhysicsMath::IsFinite(t.position) || !PhysicsMath::IsFinite(t.rotation) || !PhysicsMath::IsPositive(t.scale)) return false;
		if (object != this && (std::abs(t.scale.x - t.scale.y) > t.scale.x * 1e-5f ||
			std::abs(t.scale.x - t.scale.z) > t.scale.x * 1e-5f)) return false;
	}
	const auto world = GetWorldTransformMatrix();
	XMFLOAT4X4 matrix;
	XMStoreFloat4x4(&matrix, world);
	for (const auto& row : matrix.m)
		for (float value : row)
			if (!PhysicsMath::IsFinite(value)) return false;
	XMVECTOR s, q, p;
	if (!XMMatrixDecompose(&s, &q, &p, world)) return false;
	XMStoreFloat3(&position, p);
	XMStoreFloat3(&scale, s);
	XMStoreFloat4(&rotation, XMQuaternionNormalize(q));
	return PhysicsMath::IsFinite(position) && PhysicsMath::IsPositive(scale) &&
		PhysicsMath::IsFinite(rotation.x) && PhysicsMath::IsFinite(rotation.y) &&
		PhysicsMath::IsFinite(rotation.z) && PhysicsMath::IsFinite(rotation.w);
}

b3BodyId GameObject::GetPhysicsBody() const noexcept
{
	if (auto* body = GetComponent<Rigidbody>()) return body->bodyId;
	if (auto* collider = GetComponent<Collider>()) return collider->standaloneBodyId;
	return b3_nullBodyId;
}

void GameObject::RefreshPhysics() noexcept
{
	if (isPendingKill) return;
	auto* rigidbody = GetComponent<Rigidbody>();
	auto* collider = GetComponent<Collider>();
	if (!rigidbody && !collider) return;
	XMFLOAT3 position{}, scale{ 1, 1, 1 };
	XMFLOAT4 rotation{ 0, 0, 0, 1 };
	bool valid = GetPhysicsWorldPose(position, rotation, scale);
	if (!valid) { position = {}; rotation = { 0, 0, 0, 1 }; }
	if (rigidbody)
	{
		if (collider && b3Body_IsValid(collider->standaloneBodyId))
		{
			collider->ReleaseShape();
			collider->ReleaseStandaloneBody();
		}
		rigidbody->EnsureBody(position, rotation, valid);
	}
	else collider->EnsureStandaloneBody(position, rotation, valid);
	const auto body = GetPhysicsBody();
	if (valid)
	{
		b3Body_SetTransform(body, PhysicsMath::Vector(position), PhysicsMath::Quaternion(rotation));
		if (collider) valid = collider->AttachShape(body, scale);
	}
	if (rigidbody) rigidbody->suspended = !valid;
	if (collider) collider->suspended = !valid;
	if (!valid)
	{
		if (b3Body_IsEnabled(body)) b3Body_Disable(body);
		return;
	}
	if (!b3Body_IsEnabled(body)) b3Body_Enable(body);
	if (rigidbody && rigidbody->bodyType == Rigidbody::Type::Dynamic) b3Body_SetAwake(body, true);
}

void GameObject::NotifyPhysicsTransformChanged() noexcept
{
	hasPhysicsPose = false;
	RefreshPhysics();
	for (auto& child : children) child->NotifyPhysicsTransformChanged();
}

void GameObject::CapturePhysicsPose() noexcept
{
	hasPhysicsPose = false;
	if (isPendingKill) return;
	const auto body = GetPhysicsBody();
	if (!b3Body_IsValid(body) || !b3Body_IsEnabled(body) || b3Body_GetUserData(body) != this) return;
	const auto p = b3Body_GetPosition(body);
	const auto q = b3Body_GetRotation(body);
	physicsPosition = { p.x, p.y, p.z };
	physicsRotation = { q.v.x, q.v.y, q.v.z, q.s };
	hasPhysicsPose = true;
}

void GameObject::ApplyPhysicsPose() noexcept
{
	if (!hasPhysicsPose || isPendingKill) return;
	hasPhysicsPose = false;
	XMFLOAT3 position, scale;
	XMFLOAT4 rotation;
	if (!GetPhysicsWorldPose(position, rotation, scale)) return;
	const auto world = XMMatrixScaling(scale.x, scale.y, scale.z) *
		XMMatrixRotationQuaternion(XMQuaternionNormalize(XMLoadFloat4(&physicsRotation))) *
		XMMatrixTranslation(physicsPosition.x, physicsPosition.y, physicsPosition.z);
	const auto local = parent ? world * XMMatrixInverse(nullptr, parent->GetWorldTransformMatrix()) : world;
	const auto result = MakeTransformFromMatrix(local);
	if (!PhysicsMath::IsFinite(result.position) || !PhysicsMath::IsFinite(result.rotation)) return;
	// Simulation readback deliberately bypasses the public setters and retains local scale.
	transform.position = result.position;
	transform.rotation = result.rotation;
}
