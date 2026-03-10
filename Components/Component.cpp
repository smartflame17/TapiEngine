#include "Component.h"
#include "../Scene/GameObject.h"
#include <cassert>

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
