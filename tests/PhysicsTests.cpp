#include "PhysicsTestAccess.h"
#include "../Tools/FixedStepClock.h"
#include <box3d/box3d.h>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <type_traits>

int RunPhysicsDebugDrawTests();

static_assert(!std::is_copy_constructible_v<Physics> && !std::is_move_constructible_v<Physics>);
static_assert(!std::is_copy_assignable_v<Physics> && !std::is_move_assignable_v<Physics>);
static_assert(!std::is_default_constructible_v<Physics>);

namespace
{
int checks = 0;
void Check(bool condition, const char* message)
{
	++checks;
	if (!condition) throw std::runtime_error(message);
}

template<typename F>
bool ThrowsLogicError(F&& operation)
{
	try { operation(); }
	catch (const std::logic_error&) { return true; }
	return false;
}

b3BodyId AddFallingBox(b3WorldId world)
{
	b3BodyDef definition = b3DefaultBodyDef();
	definition.type = b3_dynamicBody;
	definition.position = { 0.0f, 10.0f, 0.0f };
	definition.enableSleep = false;
	const auto body = b3CreateBody(world, &definition);
	auto shape = b3DefaultShapeDef();
	shape.density = 1.0f;
	auto box = b3MakeBoxHull(0.5f, 0.5f, 0.5f);
	b3CreateHullShape(body, &shape, &box.base);
	return body;
}

void TestLifetime()
{
	const int baseline = b3GetWorldCount();
	Check(ThrowsLogicError([] { Physics::GetInstance(); }), "Access before construction must fail");
	b3WorldId lastWorld{};
	{
		auto physics = PhysicsTestAccess::Create();
		Check(&Physics::GetInstance() == physics.get(), "Accessor returns the owned instance");
		Check(b3GetWorldCount() == baseline + 1, "Exactly one world is owned");
		Check(ThrowsLogicError([] { PhysicsTestAccess::Create(); }), "Duplicate instances must fail");
		Check(&Physics::GetInstance() == physics.get() && b3GetWorldCount() == baseline + 1,
			"Duplicate rejection preserves the original instance and world");
		for (int i = 0; i < 8; ++i)
		{
			const auto oldWorld = PhysicsTestAccess::World(*physics);
			const auto body = AddFallingBox(oldWorld);
			physics->Step();
			physics->Reset();
			lastWorld = PhysicsTestAccess::World(*physics);
			Check(!b3World_IsValid(oldWorld) && !b3Body_IsValid(body), "Reset invalidates old world and bodies");
			Check(b3World_IsValid(lastWorld) && b3GetWorldCount() == baseline + 1, "Reset replaces without leaking");
			Check(b3World_GetCounters(lastWorld).bodyCount == 0, "Reset produces an empty world");
			const auto gravity = b3World_GetGravity(lastWorld);
			Check(gravity.x == 0 && gravity.y == -9.8f && gravity.z == 0, "Reset restores configured gravity");
			Check(&Physics::GetInstance() == physics.get(), "Reset preserves singleton identity");
			physics->Step(); // An empty world is also valid to step.
		}
	}
	Check(!b3World_IsValid(lastWorld) && b3GetWorldCount() == baseline, "Destruction releases the world");
	Check(ThrowsLogicError([] { Physics::GetInstance(); }), "Access after destruction must fail");
	try
	{
		auto physics = PhysicsTestAccess::Create();
		throw std::runtime_error("Simulated owner constructor failure");
	}
	catch (const std::runtime_error&) {}
	Check(b3GetWorldCount() == baseline && ThrowsLogicError([] { Physics::GetInstance(); }),
		"Owner construction failure unwinds Physics and unregisters it");
	auto replacement = PhysicsTestAccess::Create();
	Check(&Physics::GetInstance() == replacement.get(), "A later owner can create a fresh instance");
}

int Drain(FixedStepClock& clock)
{
	int ticks = 0;
	while (clock.ConsumeStep()) ++ticks;
	Check(clock.GetAlpha() >= 0 && clock.GetAlpha() < 1, "Interpolation alpha stays in range");
	return ticks;
}

void TestClock()
{
	for (const int fps : { 30, 60, 144 })
	{
		FixedStepClock clock(Physics::FixedTimeStep);
		int ticks = 0;
		for (int frame = 0; frame < fps * 10; ++frame)
		{
			clock.Advance(1.0 / fps);
			ticks += Drain(clock);
		}
		Check(ticks == 600, "Ten seconds produces 600 ticks at every render rate");
		Check(clock.GetAlpha() < 1e-8, "Whole seconds leave no meaningful remainder");
	}
	FixedStepClock irregular(Physics::FixedTimeStep);
	int ticks = 0;
	for (int i = 0; i < 100; ++i)
	{
		for (const double delta : { 0.002, 0.029, 0.011, 0.025, 0.003 })
		{
			irregular.Advance(delta);
			ticks += Drain(irregular);
		}
	}
	Check(ticks == 420, "Seven seconds of irregular frames produces 420 ticks");
	FixedStepClock clock(Physics::FixedTimeStep);
	clock.Advance(Physics::FixedTimeStep * 0.5);
	Check(Drain(clock) == 0 && std::abs(clock.GetAlpha() - 0.5) < 1e-9, "Partial tick is retained");
	clock.Advance(Physics::FixedTimeStep * 0.5);
	Check(Drain(clock) == 1, "Two half ticks make one tick");
	clock.Advance(Physics::FixedTimeStep * 0.5);
	clock.Advance(10.0);
	Check(Drain(clock) == 15 && clock.GetAlpha() < 1e-8, "Stall caps total accumulated time at 0.25 seconds");
	Check(Drain(clock) == 0, "Discarded stall time does not return as debt");
	clock.Advance(Physics::FixedTimeStep * 0.5);
	clock.Reset();
	Check(clock.GetAlpha() == 0 && Drain(clock) == 0, "Reset removes pending time");
	clock.Advance(Physics::FixedTimeStep * 0.5);
	Check(Drain(clock) == 0, "Reset does not carry a partial tick into the new interval");
}

void TestFallingBody()
{
	float referenceY = 0;
	for (const int fps : { 30, 60, 144 })
	{
		auto physics = PhysicsTestAccess::Create();
		const auto body = AddFallingBox(PhysicsTestAccess::World(*physics));
		FixedStepClock clock(Physics::FixedTimeStep);
		int ticks = 0;
		for (int frame = 0; frame < fps; ++frame)
		{
			clock.Advance(1.0 / fps);
			while (clock.ConsumeStep()) { physics->Step(); ++ticks; }
		}
		const float y = b3Body_GetPosition(body).y;
		const float velocity = b3Body_GetLinearVelocity(body).y;
		Check(ticks == 60 && std::abs(velocity + 9.8f) < 0.001f, "One second of gravity changes velocity by -9.8 m/s");
		// Semi-implicit integration with 240 substeps gives this small displacement bias.
		const float expectedY = 10.0f - 4.9f * (1.0f + 1.0f / 240.0f);
		Check(std::abs(y - expectedY) < 0.002f, "Falling displacement agrees with four substeps per tick");
		if (fps == 30) referenceY = y;
		else Check(std::abs(y - referenceY) < 1e-6f, "Render rate does not change the simulated trajectory");
	}
}
}

int main()
{
	try
	{
		TestLifetime();
		TestClock();
		TestFallingBody();
		checks += RunPhysicsDebugDrawTests();
		std::cout << "PASS " << checks << " physics and timing checks\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << "FAIL: " << error.what() << '\n';
		return 1;
	}
}
