#pragma once

#include <box3d/id.h>
#include <box3d/math_functions.h>

class App;
class PhysicsTestAccess;

// One App owns the active instance. All access is on the main thread.
class Physics
{
public:
	static constexpr double FixedTimeStep = 1.0 / 60.0;
	static constexpr int SubStepCount = 4;

	~Physics() noexcept;
	Physics(const Physics&) = delete;
	Physics& operator=(const Physics&) = delete;
	Physics(Physics&&) = delete;
	Physics& operator=(Physics&&) = delete;

	static Physics& GetInstance();
	void Step() noexcept;
	// The owner must destroy scene components before resetting their world.
	void Reset();
	b3Vec3 GetGravity() const noexcept;
	void SetGravity(const b3Vec3& newGravity) noexcept;

private:
	friend class App;
	friend class PhysicsTestAccess;
	friend class Rigidbody;
	friend class Collider;
	Physics();
	static b3WorldId CreateWorld();

	static Physics* instance;
	b3WorldId worldId = b3_nullWorldId;
};
