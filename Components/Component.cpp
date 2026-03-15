#include "Component.h"
#include "../Scene/GameObject.h"

std::uint64_t Component::nextId = 1;

Component::Component() :
	id(nextId++)
{}

void Component::SetOwner(GameObject* gameObject) noexcept
{
	owner = gameObject;
}

GameObject& Component::GetGameObject() const noexcept
{
	assert(owner != nullptr);
	return *owner;
}

std::uint64_t Component::GetId() const noexcept
{
	return id;
}

Transform& Component::GetTransform() noexcept
{
	return transform;
}

const Transform& Component::GetTransform() const noexcept
{
	return transform;
}

DirectX::XMMATRIX Component::GetLocalTransformMatrix() const noexcept
{
	return MakeTransformMatrix(transform);
}

DirectX::XMMATRIX Component::GetWorldTransformMatrix() const noexcept
{
	return GetLocalTransformMatrix() * GetGameObject().GetWorldTransformMatrix();
}

void Component::OnUpdate(float dt, bool isSimulationRunning) noexcept
{
}

void Component::OnInspector() noexcept
{
}
