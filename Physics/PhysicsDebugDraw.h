#pragma once

#include <box3d/types.h>
#include <cstddef>
#include <exception>
#include <vector>

struct PhysicsDebugDrawSettings
{
	bool drawDuringPlay = false;
	b3Vec3 idleColor = { 0, 0, 1 };
	b3Vec3 collisionColor = { 1, 0, 0 };
};

// Consecutive pairs are line endpoints. No borrowed Box3D geometry or native IDs.
struct PhysicsDebugDrawFrame
{
	std::vector<b3Vec3> idleVertices;
	std::vector<b3Vec3> collisionVertices;
};

// CPU adapter; its owner must destroy the world before destroying this object.
class PhysicsDebugDraw
{
public:
	void Attach(b3WorldDef& definition) noexcept;
	const PhysicsDebugDrawFrame& Collect(b3WorldId world, bool isPlayMode, const b3AABB& bounds);
	void Clear() noexcept;
	PhysicsDebugDrawSettings settings;

private:
	friend class PhysicsTestAccess;
	struct Shape;
	struct DrawContext;
	static void* CreateShape(const b3DebugShape* shape, void* context) noexcept;
	static void DestroyShape(void* shape, void* context) noexcept;
	static void DrawShape(void* shape, b3WorldTransform transform, b3HexColor color, void* context) noexcept;
	PhysicsDebugDrawFrame frame;
	std::exception_ptr callbackError;
	std::size_t shapeCount = 0;
};
