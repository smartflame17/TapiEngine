#include "Physics.h"

#include <box3d/box3d.h>
#include <stdexcept>

Physics* Physics::instance = nullptr;

Physics::Physics()
{
	if (instance != nullptr)
	{
		throw std::logic_error("Only one App-owned Physics instance may be active.");
	}
	worldId = CreateWorld();
	instance = this;
}

Physics::~Physics() noexcept
{
	b3DestroyWorld(worldId);
	worldId = b3_nullWorldId;
	instance = nullptr;
}

Physics& Physics::GetInstance()
{
	if (instance == nullptr)
	{
		throw std::logic_error("Physics is only available during its owning App's lifetime.");
	}
	return *instance;
}

b3WorldId Physics::CreateWorld()
{
	b3WorldDef definition = b3DefaultWorldDef();
	definition.gravity = { 0.0f, -9.8f, 0.0f };
	definition.workerCount = 1;
	const b3WorldId world = b3CreateWorld(&definition);
	if (!b3World_IsValid(world))
	{
		throw std::runtime_error("Failed to create the Box3D physics world.");
	}
	return world;
}

void Physics::Step() noexcept
{
	b3World_Step(worldId, static_cast<float>(FixedTimeStep), SubStepCount);
}

void Physics::Reset()
{
	// Keep the old world valid if replacement creation fails.
	const b3WorldId replacement = CreateWorld();
	b3DestroyWorld(worldId);
	worldId = replacement;
}

b3Vec3 Physics::GetGravity() const noexcept
{
	if (!b3World_IsValid(worldId))
	{
		throw std::logic_error("Physics world is not valid.");
	}
	return b3World_GetGravity(worldId);
}

void Physics::SetGravity(const b3Vec3& newGravity) noexcept
{
	if (!b3World_IsValid(worldId))
	{
		throw std::logic_error("Physics world is not valid.");
	}
	b3World_SetGravity(worldId, newGravity);
}