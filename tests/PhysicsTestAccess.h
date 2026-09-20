#pragma once

#include "../Physics/Physics.h"
#include <memory>

// Only test fixtures may reach native handles; the engine API does not expose them.
class PhysicsTestAccess
{
public:
	static std::unique_ptr<Physics> Create() { return std::unique_ptr<Physics>(new Physics); }
	static b3WorldId World(const Physics& physics) { return physics.worldId; }
	static std::size_t DebugShapeCount(const Physics& physics) { return physics.debugDraw.shapeCount; }
};
