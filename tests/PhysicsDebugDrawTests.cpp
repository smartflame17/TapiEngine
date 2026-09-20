#include "PhysicsTestAccess.h"
#include <box3d/box3d.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace
{
int checks = 0;
void Check(bool value, const char* message) { ++checks; if (!value) throw std::runtime_error(message); }
bool Near(float a, float b) { return std::abs(a - b) < 0.0001f; }
const b3AABB Bounds{ { -1000, -1000, -1000 }, { 1000, 1000, 1000 } };
constexpr std::size_t Counts[] = { 24, 192, 264 };

b3ShapeId AddShape(b3BodyId body, int type, b3Vec3 center = b3Vec3_zero)
{
	auto def = b3DefaultShapeDef();
	if (type == 0)
	{
		auto box = b3MakeTransformedBoxHull(0.5f, 0.5f, 0.5f, { center, b3Quat_identity });
		return b3CreateHullShape(body, &def, &box.base);
	}
	if (type == 1)
	{
		const b3Sphere sphere{ center, 0.5f };
		return b3CreateSphereShape(body, &def, &sphere);
	}
	const b3Capsule capsule{ b3Add(center, { 0, -0.5f, 0 }), b3Add(center, { 0, 0.5f, 0 }), 0.5f };
	return b3CreateCapsuleShape(body, &def, &capsule);
}

b3BodyId AddBody(Physics& physics, b3Vec3 position = b3Vec3_zero, bool dynamic = false)
{
	auto def = b3DefaultBodyDef();
	def.position = b3ToPos(position);
	def.type = dynamic ? b3_dynamicBody : b3_staticBody;
	return b3CreateBody(PhysicsTestAccess::World(physics), &def);
}

void Geometry()
{
	for (int type = 0; type < 3; ++type)
	{
		auto physics = PhysicsTestAccess::Create();
		const auto body = AddBody(*physics, { 200, 3, 8 });
		const b3Vec3 center{ 1, 2, -1 };
		const auto shape = AddShape(body, type, center);
		const auto rotation = b3NormalizeQuat({ { 0.2f, -0.3f, 0.1f }, 0.9f });
		b3Body_SetTransform(body, { 200, 3, 8 }, rotation);
		Check(PhysicsTestAccess::DebugShapeCount(*physics) == 0, "Debug geometry is created lazily");
		const auto& frame = physics->CollectDebugDraw(false, Bounds);
		Check(frame.idleVertices.size() == Counts[type] && frame.collisionVertices.empty(), "Expected collider wireframe topology");
		Check(PhysicsTestAccess::DebugShapeCount(*physics) == 1, "One cache per drawn shape");
		const b3Transform transform{ { 200, 3, 8 }, rotation };
		for (const auto point : frame.idleVertices)
		{
			const auto local = b3Sub(b3InvTransformPoint(transform, point), center);
			if (type == 0)
				Check(Near(std::abs(local.x), 0.5f) && Near(std::abs(local.y), 0.5f) && Near(std::abs(local.z), 0.5f), "Hull endpoints preserve offset and rotation");
			else
			{
				const float y = type == 1 ? local.y : local.y - std::clamp(local.y, -0.5f, 0.5f);
				Check(Near(local.x * local.x + y * y + local.z * local.z, 0.25f), "Curved wireframe endpoints lie on native surface");
			}
		}
		const auto pose = b3Body_GetPosition(body);
		const auto velocity = b3Body_GetLinearVelocity(body);
		for (int i = 0; i < 4; ++i) physics->CollectDebugDraw(false, Bounds);
		Check(b3Body_GetPosition(body).x == pose.x && b3Body_GetPosition(body).y == pose.y &&
			b3Body_GetLinearVelocity(body).y == velocity.y, "Drawing does not advance physics");
		Check(PhysicsTestAccess::DebugShapeCount(*physics) == 1, "Repeated draws reuse local geometry");
		Check(physics->CollectDebugDraw(false, { { -10, -10, -10 }, { 10, 10, 10 } }).idleVertices.empty(), "Drawing honors caller bounds");
		b3Body_Disable(body);
		Check(physics->CollectDebugDraw(false, Bounds).idleVertices.empty(), "Disabled bodies are not drawn");
		b3Body_Enable(body);
		Check(physics->CollectDebugDraw(false, Bounds).idleVertices.size() == Counts[type], "Re-enabled bodies draw again");
		b3DestroyShape(shape, true);
		Check(PhysicsTestAccess::DebugShapeCount(*physics) == 0, "Shape removal immediately releases cached geometry");
		Check(physics->CollectDebugDraw(false, Bounds).idleVertices.empty(), "Removed collider leaves no lines");
	}
}

void Overlaps()
{
	for (int a = 0; a < 3; ++a)
		for (int b = a; b < 3; ++b)
		{
			auto physics = PhysicsTestAccess::Create();
			const auto first = AddBody(*physics), second = AddBody(*physics, { 0.25f, 0, 0 });
			AddShape(first, a); AddShape(second, b);
			const auto total = Counts[a] + Counts[b];
			const auto& overlap = physics->CollectDebugDraw(false, Bounds);
			Check(overlap.idleVertices.empty() && overlap.collisionVertices.size() == total, "All static shape pairs turn red without stepping");
			b3Body_SetTransform(second, { 4, 0, 0 }, b3Quat_identity);
			const auto& separate = physics->CollectDebugDraw(false, Bounds);
			Check(separate.idleVertices.size() == total && separate.collisionVertices.empty(), "All shape pairs return to blue immediately after separation");
			b3DestroyBody(second);
			AddShape(first, b);
			Check(physics->CollectDebugDraw(false, Bounds).collisionVertices.empty(), "Shapes on the same body do not collide with themselves");
		}
	auto physics = PhysicsTestAccess::Create();
	const auto first = AddBody(*physics), second = AddBody(*physics, { 0.8f, 0.8f, 0 });
	AddShape(first, 1); AddShape(second, 1);
	Check(physics->CollectDebugDraw(false, Bounds).collisionVertices.empty(), "Overlapping sphere AABBs do not imply geometric overlap");
	b3Body_SetTransform(second, { 0.8f, 0, 0 }, b3Quat_identity);
	const auto third = AddBody(*physics, { -0.8f, 0, 0 }); AddShape(third, 1);
	Check(physics->CollectDebugDraw(false, Bounds).collisionVertices.size() == 3 * Counts[1], "Multiple simultaneous overlaps color every participant");
	b3DestroyBody(second);
	Check(physics->CollectDebugDraw(false, Bounds).collisionVertices.size() == 2 * Counts[1], "Removing one overlap preserves another");
}

void ContactsAndLifetime()
{
	auto physics = PhysicsTestAccess::Create();
	auto& settings = physics->GetDebugDrawSettings();
	Check(!settings.drawDuringPlay && settings.idleColor.z == 1 && settings.idleColor.x == 0 &&
		settings.collisionColor.x == 1 && settings.collisionColor.z == 0, "Default settings are blue/red and hidden during Play");
	const auto floor = AddBody(*physics, { 0, -0.5f, 0 }); AddShape(floor, 0);
	const auto ball = AddBody(*physics, { 0, 2, 0 }, true); const auto sphere = AddShape(ball, 1);
	physics->CollectDebugDraw(false, Bounds);
	const auto& hidden = physics->CollectDebugDraw(true, Bounds);
	Check(hidden.idleVertices.empty() && hidden.collisionVertices.empty(), "Play clears previous Edit lines while disabled");
	settings.drawDuringPlay = true;
	Check(physics->CollectDebugDraw(true, Bounds).collisionVertices.empty(), "No simulation contact before stepping");
	for (int i = 0; i < 300; ++i) physics->Step();
	Check(physics->CollectDebugDraw(true, Bounds).collisionVertices.size() == Counts[0] + Counts[1], "Resting contact colors both bodies red");
	Check(!b3Body_IsAwake(ball), "Resting body can sleep with debug drawing");
	const auto pausedY = b3Body_GetPosition(ball).y;
	for (int i = 0; i < 4; ++i)
		Check(physics->CollectDebugDraw(true, Bounds).collisionVertices.size() == Counts[0] + Counts[1], "Paused drawing retains sleeping contact colors");
	Check(b3Body_GetPosition(ball).y == pausedY && !b3Body_IsAwake(ball), "Drawing does not move or wake sleeping bodies");
	b3Body_SetTransform(ball, { 0, 5, 0 }, b3Quat_identity);
	b3Body_SetAwake(ball, true); // GameObject transform setters also wake dynamic bodies.
	Check(physics->CollectDebugDraw(true, Bounds).collisionVertices.size() == Counts[0] + Counts[1], "Paused transform edits retain last simulation contacts");
	Check(physics->CollectDebugDraw(false, Bounds).collisionVertices.empty(), "Edit mode ignores stale simulation contacts");
	physics->Step();
	Check(physics->CollectDebugDraw(true, Bounds).collisionVertices.empty(), "Resuming simulation clears separated contacts");
	const b3Sphere edited{ { 1, 0, 0 }, 0.75f };
	b3Shape_SetSphere(sphere, &edited);
	Check(PhysicsTestAccess::DebugShapeCount(*physics) == 1, "Native geometry update invalidates cached shape");
	Check(physics->CollectDebugDraw(true, Bounds).idleVertices.size() == Counts[0] + Counts[1], "Updated shape builds fresh geometry");
	settings.idleColor = { 0.2f, 0.3f, 0.4f };
	const auto oldWorld = PhysicsTestAccess::World(*physics);
	physics->Reset();
	Check(!b3World_IsValid(oldWorld) && PhysicsTestAccess::DebugShapeCount(*physics) == 0, "Reset destroys all cached geometry with the old world");
	Check(settings.drawDuringPlay && settings.idleColor.x == 0.2f, "Reset preserves session settings");
	Check(physics->CollectDebugDraw(false, Bounds).idleVertices.empty(), "Reset clears frame lists");
	AddShape(AddBody(*physics), 2);
	Check(physics->CollectDebugDraw(false, Bounds).idleVertices.size() == Counts[2], "Replacement world installs debug callbacks");
	// Destructor must release this last cached shape through the world callback.
}
}

int RunPhysicsDebugDrawTests()
{
	Geometry(); Overlaps(); ContactsAndLifetime();
	return checks;
}
