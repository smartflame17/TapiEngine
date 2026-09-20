#include "PhysicsTestAccess.h"
#include "PhysicsComponentTestAccess.h"
#include "../Scene/GameObject.h"
#include "../imgui/imgui_internal.h"
#include <box3d/box3d.h>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

using Access = PhysicsComponentTestAccess;
using namespace DirectX;
namespace
{
int checks = 0;
void Check(bool value, const char* message) { ++checks; if (!value) throw std::runtime_error(message); }
bool Near(float a, float b, float tolerance = 0.002f) { return std::abs(a - b) < tolerance; }
b3WorldId World() { return PhysicsTestAccess::World(Physics::GetInstance()); }
void Tick(Scene& scene, int count = 1)
{
	for (int i = 0; i < count; ++i) { Physics::GetInstance().Step(); scene.SynchronizePhysicsTransforms(); }
}
void CheckPose(GameObject& object, b3BodyId body)
{
	XMFLOAT4X4 world;
	XMStoreFloat4x4(&world, object.GetWorldTransformMatrix());
	const auto p = b3Body_GetPosition(body);
	Check(Near(world._41, p.x) && Near(world._42, p.y) && Near(world._43, p.z), "World position matches body");
	XMVECTOR scale, rotation, position;
	Check(XMMatrixDecompose(&scale, &rotation, &position, object.GetWorldTransformMatrix()), "World pose decomposes");
	const auto q = b3Body_GetRotation(body);
	Check(std::abs(XMVectorGetX(XMVector4Dot(rotation, XMVectorSet(q.v.x, q.v.y, q.v.z, q.s)))) > 0.99999f,
		"Normalized DirectX and Box3D rotations agree");
	const auto axis = b3RotateVector(q, { 1, 0, 0 });
	XMFLOAT3 dxAxis;
	XMStoreFloat3(&dxAxis, XMVector3Normalize(XMVectorSet(world._11, world._12, world._13, 0)));
	Check(Near(axis.x, dxAxis.x) && Near(axis.y, dxAxis.y) && Near(axis.z, dxAxis.z), "Box3D rotates vectors in the same convention as DirectX");
}

void TestDebugDrawingComponents()
{
	Scene scene;
	auto& physics = Physics::GetInstance();
	const b3AABB bounds{ { -100, -100, -100 }, { 100, 100, 100 } };
	auto& parent = scene.CreateGameObject("debug parent");
	parent.SetPosition(10, 2, 5);
	parent.SetScale(2, 2, 2);
	parent.SetRotation(0.2f, 0.3f, -0.1f);
	auto& child = scene.CreateChildGameObject(parent, "debug collider");
	child.SetPosition(1, 2, 3);
	child.SetRotation(-0.1f, 0.5f, 0.4f);
	child.SetScale(2, 3, 4);
	auto& collider = child.AddComponent<Collider>();
	for (const auto kind : { Collider::Shape::Box, Collider::Shape::Sphere, Collider::Shape::Capsule })
	{
		auto geometry = collider.GetGeometry();
		geometry.shape = kind;
		geometry.center = { 0.25f, 0.5f, -0.5f };
		geometry.size = { 1, 2, 3 };
		geometry.radius = 0.75f;
		geometry.height = 5;
		collider.SetGeometry(geometry);
		const auto& frame = physics.CollectDebugDraw(false, bounds);
		Check(!frame.idleVertices.empty() && frame.collisionVertices.empty(), "Collider-only child renders independently of meshes");
		const auto body = Access::Body(collider);
		const b3Transform pose = b3ToRelativeTransform(b3Body_GetTransform(body), b3Pos_zero);
		const b3Vec3 center{ 1, 3, -4 }; // Local center times object and uniform parent scale.
		for (const auto point : frame.idleVertices)
		{
			const auto local = b3Sub(b3InvTransformPoint(pose, point), center);
			if (kind == Collider::Shape::Box)
				Check(Near(std::abs(local.x), 2) && Near(std::abs(local.y), 6) && Near(std::abs(local.z), 12), "Debug hull uses effective scaled box dimensions");
			else
			{
				const float y = kind == Collider::Shape::Sphere ? local.y : local.y - std::clamp(local.y, -9.0f, 9.0f);
				Check(Near(local.x * local.x + y * y + local.z * local.z, 36, 0.002f), "Debug curves preserve native radius and capsule height scaling");
			}
		}
	}
	auto geometry = collider.GetGeometry();
	geometry.height = 0.1f;
	collider.SetGeometry(geometry);
	Check(physics.CollectDebugDraw(false, bounds).idleVertices.size() == 192, "Clamped capsule draws the actual sphere primitive");
	parent.SetScale(2, 3, 2);
	Check(collider.IsPhysicsSuspended() && physics.CollectDebugDraw(false, bounds).idleVertices.empty(), "Unsupported ancestor scale suspends debug drawing");
	parent.SetScale(2, 2, 2);
	Check(physics.CollectDebugDraw(false, bounds).idleVertices.size() == 192, "Correcting scale restores debug drawing");
	child.AddComponent<Rigidbody>();
	Check(PhysicsTestAccess::DebugShapeCount(physics) == 0, "Body replacement releases old debug geometry");
	Check(physics.CollectDebugDraw(false, bounds).idleVertices.size() == 192, "Rigidbody attachment preserves collider drawing");
	child.RemoveComponent<Collider>();
	Check(physics.CollectDebugDraw(false, bounds).idleVertices.empty(), "Pending collider removal immediately removes debug lines");
	child.AddComponent<Collider>();
	Check(physics.CollectDebugDraw(false, bounds).idleVertices.size() == 24, "Same-frame replacement draws only new geometry");
	parent.Destroy();
	Check(PhysicsTestAccess::DebugShapeCount(physics) == 0 && physics.CollectDebugDraw(false, bounds).idleVertices.empty(), "Subtree destruction releases debug resources immediately");
}

void TestOwnership()
{
	Scene scene;
	for (bool colliderFirst : { false, true })
	{
		auto& go = scene.CreateGameObject("ownership");
		go.SetPosition(1, 4, 2);
		b3BodyId standalone = b3_nullBodyId;
		if (colliderFirst) standalone = Access::Body(go.AddComponent<Collider>());
		auto& rb = go.AddComponent<Rigidbody>();
		if (!colliderFirst) go.AddComponent<Collider>();
		auto& collider = *go.GetComponent<Collider>();
		Check(rb.HasCollider() && rb.GetType() == Rigidbody::Type::Static, "Default static body has collider in either attachment order");
		Check(!b3Body_IsValid(standalone), "Attaching Rigidbody replaces standalone body");
		Check(b3Body_GetUserData(Access::Body(rb)) == &go, "Body user data is owning GameObject");
		const auto defaults = b3DefaultBodyDef();
		const auto shapeDefaults = b3DefaultShapeDef();
		Check(rb.GetGravityScale() == defaults.gravityScale && rb.GetLinearDamping() == defaults.linearDamping &&
			rb.GetAngularDamping() == defaults.angularDamping && collider.GetDensity() == shapeDefaults.density &&
			collider.GetFriction() == shapeDefaults.baseMaterial.friction && collider.GetRestitution() == shapeDefaults.baseMaterial.restitution,
			"Initial tuning matches the vendored library defaults");
		go.SetStatic(true);
		Check(rb.GetType() == Rigidbody::Type::Static, "Renderer static flag is separate from body type");
		Check(b3Body_GetShapeCount(Access::Body(rb)) == 1, "Exactly one attached shape");
		bool duplicate = false;
		try { go.AddComponent<Rigidbody>(); } catch (const std::logic_error&) { duplicate = true; }
		Check(duplicate, "Duplicate Rigidbody rejected");
		duplicate = false;
		try { go.AddComponent<Collider>(); } catch (const std::logic_error&) { duplicate = true; }
		Check(duplicate, "Duplicate Collider rejected");
		rb.SetGravityScale(0.5f); rb.SetLinearDamping(0.2f); rb.SetAngularDamping(0.3f);
		Rigidbody::MotionLocks locks; locks.angularX = locks.linearZ = true; rb.SetMotionLocks(locks);
		auto oldBody = Access::Body(rb);
		rb.SetType(Rigidbody::Type::Dynamic);
		Check(!b3Body_IsValid(oldBody) && b3Body_GetType(Access::Body(rb)) == b3_dynamicBody, "Type change destroys and recreates body");
		Check(Near(b3Body_GetGravityScale(Access::Body(rb)), 0.5f) && b3Body_GetMotionLocks(Access::Body(rb)).linearZ,
			"Type change retains tuning and locks");
		Check(go.IsStatic() && Near(b3Body_GetLinearDamping(Access::Body(rb)), 0.2f) && Near(b3Body_GetAngularDamping(Access::Body(rb)), 0.3f),
			"Type change retains damping and leaves renderer static flag unchanged");
		b3Body_SetLinearVelocity(Access::Body(rb), { 2, 3, 0 });
		Tick(scene);
		const auto pose = b3Body_GetPosition(Access::Body(rb));
		rb.SetType(Rigidbody::Type::Static); rb.SetType(Rigidbody::Type::Dynamic);
		Check(Near(b3Body_GetPosition(Access::Body(rb)).y, pose.y) && b3Body_GetLinearVelocity(Access::Body(rb)).y == 0,
			"Type replacement preserves current pose and resets motion");
		const auto oldShape = Access::Shape(collider);
		go.RemoveComponent<Collider>();
		Check(!b3Shape_IsValid(oldShape) && !rb.HasCollider() && go.GetComponent<Collider>() == nullptr, "Pending collider detaches immediately");
		Check(static_cast<const GameObject&>(go).GetComponent<Collider>() == nullptr, "Const lookup ignores pending removal");
		auto& replacement = go.AddComponent<Collider>();
		Check(rb.HasCollider(), "Collider may be re-added before cleanup");
		oldBody = Access::Body(rb);
		go.RemoveComponent<Rigidbody>();
		Check(!b3Body_IsValid(oldBody) && b3Body_IsValid(Access::Body(replacement)), "Removing Rigidbody restores standalone collider immediately");
		Check(b3Body_GetUserData(Access::Body(replacement)) == &go, "Fallback has owner user data");
		auto& replacementBody = go.AddComponent<Rigidbody>();
		scene.CleanupPendingComponentRemovals();
		Check(b3Body_GetShapeCount(Access::Body(replacementBody)) == 1, "Pending old destructors do not remove replacement resources");
		go.Destroy();
		Check(!b3Body_IsValid(Access::Body(replacementBody)), "Destroyed object releases native body immediately");
		scene.CleanupDestroyedObjects();
	}
	Check(b3World_GetCounters(World()).bodyCount == 0, "Attachment/removal cycles leak no bodies");
	auto& root = scene.CreateGameObject("root"); root.AddComponent<Rigidbody>(); root.AddComponent<Collider>();
	auto& child = scene.CreateChildGameObject(root, "child"); child.AddComponent<Collider>(); child.AddComponent<Rigidbody>();
	scene.CreateChildGameObject(child, "grandchild").AddComponent<Collider>();
	root.Destroy();
	Check(b3World_GetCounters(World()).bodyCount == 0, "Destroying a subtree creates no fallback bodies");
	scene.Clear();
	for (int i = 0; i < 8; ++i)
	{
		scene.CreateGameObject("reset").AddComponent<Collider>();
		scene.Clear(); Physics::GetInstance().Reset();
		Check(b3World_GetCounters(World()).bodyCount == 0, "Reset after component cleanup leaves empty world");
	}
}

void TestGeometryAndContacts()
{
	Scene scene;
	auto& floor = scene.CreateGameObject("standalone floor"); floor.SetPosition(0, -0.5f, 0);
	auto& ground = floor.AddComponent<Collider>();
	auto geometry = ground.GetGeometry(); geometry.size = { 30, 1, 30 }; ground.SetGeometry(geometry);
	Check(!floor.GetComponent<Rigidbody>(), "Floor needs no Rigidbody component");
	int index = 0;
	for (auto shape : { Collider::Shape::Box, Collider::Shape::Sphere, Collider::Shape::Capsule })
	{
		auto& go = scene.CreateGameObject("falling shape"); go.SetPosition(static_cast<float>(index++ * 3), 5, 0);
		auto& rb = go.AddComponent<Rigidbody>(); rb.SetType(Rigidbody::Type::Dynamic);
		auto& c = go.AddComponent<Collider>();
		geometry = c.GetGeometry(); geometry.shape = shape; c.SetGeometry(geometry);
		Rigidbody::MotionLocks locks; locks.angularX = locks.angularY = locks.angularZ = true; rb.SetMotionLocks(locks);
		Tick(scene, 360);
		const float expected = shape == Collider::Shape::Capsule ? 1.0f : 0.5f;
		Check(Near(go.GetTransform().position.y, expected, 0.08f), "Box, sphere and capsule settle on standalone collider");
		CheckPose(go, Access::Body(rb));
		Check(!b3Body_IsAwake(Access::Body(rb)), "Settled body sleeps");
		const float mass = b3Body_GetMass(Access::Body(rb));
		c.SetDensity(c.GetDensity() * 2);
		Check(Near(b3Body_GetMass(Access::Body(rb)), mass * 2), "Density updates body mass");
		c.SetFriction(0.75f); c.SetRestitution(0.8f);
		Check(Near(b3Shape_GetFriction(Access::Shape(c)), 0.75f) && Near(b3Shape_GetRestitution(Access::Shape(c)), 0.8f), "Material edits reach shape");
		go.SetScale(2, 3, 4);
		Check(b3Body_GetMass(Access::Body(rb)) > mass * 2 && b3Body_IsAwake(Access::Body(rb)), "Scale rebuilds mass and wakes body");
		geometry.center = { 1, 2, 3 }; c.SetGeometry(geometry);
		if (shape == Collider::Shape::Sphere)
		{
			const auto sphere = b3Shape_GetSphere(Access::Shape(c));
			Check(Near(sphere.radius, 2) && Near(sphere.center.x, 2) && Near(sphere.center.y, 6) && Near(sphere.center.z, 12), "Sphere uses largest scale and scaled center");
		}
		if (shape == Collider::Shape::Capsule)
		{
			const auto capsule = b3Shape_GetCapsule(Access::Shape(c));
			Check(Near(capsule.radius, 2) && Near(capsule.center2.y - capsule.center1.y + 2 * capsule.radius, 6), "Capsule scales radius in X/Z and height in Y");
			go.SetScale(4, 0.1f, 4);
			Check(Near(b3Shape_GetSphere(Access::Shape(c)).radius, 2), "Short capsule becomes sphere at its diameter");
		}
		const auto saved = Access::Shape(c);
		geometry.radius = std::numeric_limits<float>::quiet_NaN();
		Check(!c.SetGeometry(geometry) && b3Shape_IsValid(saved), "Non-finite geometry rejected without destroying shape");
		Check(!c.SetDensity(0) && !c.SetFriction(-1) && !c.SetRestitution(2), "Invalid material ranges rejected");
		Check(!rb.SetGravityScale(std::numeric_limits<float>::infinity()) && !rb.SetLinearDamping(-1) && !rb.SetAngularDamping(-1), "Invalid body tuning rejected");
		go.Destroy(); scene.CleanupDestroyedObjects();
	}
}

void TestTransforms()
{
	Scene scene;
	auto& parent = scene.CreateGameObject("parent");
	parent.SetTransform({ { 3, 2, -1 }, { 0.2f, -0.5f, 0.1f }, { 2, 2, 2 } });
	auto& child = scene.CreateChildGameObject(parent, "child");
	auto& rb = child.AddComponent<Rigidbody>(); child.AddComponent<Collider>();
	child.SetTransform({ { 1, 2, 3 }, { 0.3f, -0.4f, 0.5f }, { 0.5f, 2, 3 } });
	CheckPose(child, Access::Body(rb));
	for (int i = 0; i < 80; ++i)
	{
		child.SetRotation(i * 0.087f, i * -0.31f, i * 0.23f);
		const auto before = b3Body_GetRotation(Access::Body(rb));
		Tick(scene, 3);
		CheckPose(child, Access::Body(rb));
		const auto after = b3Body_GetRotation(Access::Body(rb));
		Check(before.v.x == after.v.x && before.s == after.s, "Readback never writes back into native rotation");
		Check(child.GetTransform().scale.y == 2, "Readback preserves exact local scale");
	}
	parent.SetPosition(5, 6, 7); CheckPose(child, Access::Body(rb));
	parent.SetRotation(0.1f, 0.2f, 0.3f); CheckPose(child, Access::Body(rb));
	parent.SetScale(3, 3, 3); CheckPose(child, Access::Body(rb));
	parent.SetScale(2, 1, 1);
	Check(rb.IsPhysicsSuspended() && !b3Body_IsEnabled(Access::Body(rb)), "Nonuniform ancestor suspends child");
	Tick(scene, 2);
	parent.SetScale(1, 1, 1);
	Check(!rb.IsPhysicsSuspended() && b3Body_IsEnabled(Access::Body(rb)), "Correcting ancestor resumes physics");
	child.SetScale(-1, 1, 1); Check(rb.IsPhysicsSuspended(), "Negative object scale suspends physics");
	child.SetScale(1, 1, 1); CheckPose(child, Access::Body(rb));
	child.SetPosition(std::numeric_limits<float>::quiet_NaN(), 1, 1);
	Check(rb.IsPhysicsSuspended(), "NaN position suspends physics even with fast floating point");
	child.SetPosition(1, 2, 3); Check(!rb.IsPhysicsSuspended(), "Finite pose restores physics");
	parent.SetScale(1e30f, 1e30f, 1e30f); child.SetScale(1e30f, 1e30f, 1e30f);
	Check(rb.IsPhysicsSuspended(), "Overflowing composed world transform suspends physics");
	parent.SetScale(1, 1, 1); child.SetScale(1, 1, 1);
	auto& other = scene.CreateGameObject("new parent"); other.SetPosition(10, 0, 0);
	other.AddChild(parent.DetachChild(child));
	Check(child.GetTransform().position.x == 1 && Near(b3Body_GetPosition(Access::Body(rb)).x, 11), "Reparent preserves local semantics and updates body immediately");
	Access::Select(scene, child);
	const Transform desired{ { 8, 9, 10 }, { 0.3f, 0.4f, 0.5f }, { 2, 3, 4 } };
	scene.SetSelectedWorldTransformMatrix(MakeTransformMatrix(desired));
	CheckPose(child, Access::Body(rb));
	Check(Near(b3Body_GetPosition(Access::Body(rb)).x, 8), "Gizmo world edit reaches body immediately");

	auto& moving = scene.CreateGameObject("moving parent");
	auto& movingBody = moving.AddComponent<Rigidbody>(); moving.AddComponent<Collider>();
	movingBody.SetType(Rigidbody::Type::Dynamic); movingBody.SetGravityScale(0);
	auto& sleeping = scene.CreateChildGameObject(moving, "sleeping child"); sleeping.SetPosition(0, 10, 0);
	auto& sleepingBody = sleeping.AddComponent<Rigidbody>(); sleeping.AddComponent<Collider>();
	sleepingBody.SetType(Rigidbody::Type::Dynamic); sleepingBody.SetGravityScale(0);
	b3Body_SetAwake(Access::Body(sleepingBody), false);
	auto& staticChild = scene.CreateChildGameObject(sleeping, "static grandchild"); staticChild.SetPosition(0, 5, 0);
	auto& staticCollider = staticChild.AddComponent<Collider>();
	b3Body_SetLinearVelocity(Access::Body(movingBody), { 1, 0, 0 });
	Tick(scene, 60);
	Check(Near(moving.GetTransform().position.x, 1), "Dynamic parent moves");
	CheckPose(sleeping, Access::Body(sleepingBody)); CheckPose(staticChild, Access::Body(staticCollider));
	Check(Near(b3Body_GetPosition(Access::Body(sleepingBody)).x, 0) && !b3Body_IsAwake(Access::Body(sleepingBody)), "Sleeping child retains independent world pose and sleep state");
	moving.SetPosition(3, 0, 0);
	Check(Near(b3Body_GetPosition(Access::Body(sleepingBody)).x, 2) && b3Body_IsAwake(Access::Body(sleepingBody)), "Explicit parent edit moves and wakes descendants");
	CheckPose(staticChild, Access::Body(staticCollider));
	staticChild.SetScale(0, 1, 1);
	Check(staticCollider.IsPhysicsSuspended() && !b3Body_IsEnabled(Access::Body(staticCollider)), "Standalone colliders also suspend");
	staticChild.SetScale(1, 1, 1); Check(!staticCollider.IsPhysicsSuspended(), "Standalone collider recovers");

	// Exercise changing native rotations, including a dynamic child under a rotating parent.
	auto& rotating = scene.CreateGameObject("rotating parent"); rotating.SetPosition(-20, 0, 0);
	auto& rotatingBody = rotating.AddComponent<Rigidbody>(); rotating.AddComponent<Collider>();
	rotatingBody.SetType(Rigidbody::Type::Dynamic); rotatingBody.SetGravityScale(0);
	auto& nested = scene.CreateChildGameObject(rotating, "independent dynamic child"); nested.SetPosition(0, 8, 0);
	auto& nestedBody = nested.AddComponent<Rigidbody>(); nested.AddComponent<Collider>();
	nestedBody.SetType(Rigidbody::Type::Dynamic); nestedBody.SetGravityScale(0);
	b3Body_SetAngularVelocity(Access::Body(rotatingBody), { 0.7f, 1.2f, -0.4f });
	b3Body_SetAngularVelocity(Access::Body(nestedBody), { -0.5f, 0.3f, 1.1f });
	for (int i = 0; i < 240; ++i)
	{
		Tick(scene);
		CheckPose(rotating, Access::Body(rotatingBody)); CheckPose(nested, Access::Body(nestedBody));
	}
	Check(Near(b3Body_GetPosition(Access::Body(nestedBody)).x, -20) && Near(b3Body_GetPosition(Access::Body(nestedBody)).y, 8),
		"Rotating parent never drags independent dynamic child");
}

void TestInspector()
{
	Scene scene;
	auto& go = scene.CreateGameObject("inspector");
	auto& rb = go.AddComponent<Rigidbody>();
	ImGui::CreateContext();
	auto& io = ImGui::GetIO(); io.IniFilename = nullptr; io.DisplaySize = { 2000, 1000 }; io.DeltaTime = 1.0f / 60;
	unsigned char* pixels; int w, h; io.Fonts->GetTexDataAsRGBA32(&pixels, &w, &h);
	ImGui::NewFrame(); ImGui::Begin("Physics controls");
	ImGui::LogToBuffer(); Access::Inspector(rb);
	Check(std::string(ImGui::GetCurrentContext()->LogBuffer.c_str()).find("no active Collider") != std::string::npos, "Inspector visibly warns about missing collider");
	ImGui::LogFinish(); ImGui::End(); ImGui::Render();
	auto& collider = go.AddComponent<Collider>();
	ImGui::NewFrame(); ImGui::Begin("Physics controls"); ImGui::LogToBuffer();
	Access::Inspector(rb); Access::Inspector(collider);
	Check(std::string(ImGui::GetCurrentContext()->LogBuffer.c_str()).find("no active Collider") == std::string::npos, "Missing-collider warning clears when attached");
	ImGui::LogFinish(); ImGui::End(); ImGui::Render();
	go.SetScale(0, 1, 1);
	ImGui::NewFrame(); ImGui::Begin("Physics controls"); ImGui::LogToBuffer(); Access::Inspector(rb); Access::Inspector(collider);
	Check(std::string(ImGui::GetCurrentContext()->LogBuffer.c_str()).find("Physics suspended") != std::string::npos, "Unsupported transform warning is visible");
	ImGui::LogFinish(); ImGui::End(); ImGui::Render();
	go.SetScale(1, 1, 1);
	Access::Select(scene, go);
	auto frame = [&](ImGuiID activate = 0)
	{
		ImGui::NewFrame();
		if (activate)
		{
			auto& context = *ImGui::GetCurrentContext();
			context.NavActivateId = activate;
			context.NavActivateFlags = ImGuiActivateFlags_PreferInput;
		}
		scene.DrawInspectorWindow();
		ImGui::Render();
	};
	frame(); frame();
	const auto windowId = ImGui::FindWindowByName("Inspector")->ID;
	auto vectorFieldId = [&](const char* label)
	{
		const int axis = 0;
		const auto group = ImHashStr(label, 0, windowId);
		return ImHashStr("", 0, ImHashData(&axis, sizeof(axis), group));
	};
	auto enterValue = [&](ImGuiID id, const char* text)
	{
		frame(id);
		Check(ImGui::GetCurrentContext()->ActiveId == id, "Inspector field accepts text input");
		io.AddInputCharactersUTF8(text); frame();
		io.AddKeyEvent(ImGuiKey_Enter, true); frame();
		io.AddKeyEvent(ImGuiKey_Enter, false); frame();
	};
	enterValue(vectorFieldId("Position"), "7.5");
	Check(Near(go.GetTransform().position.x, 7.5f) && Near(b3Body_GetPosition(Access::Body(rb)).x, 7.5f), "Inspector position edit immediately updates body");
	enterValue(vectorFieldId("Rotation"), "35");
	Check(Near(go.GetTransform().rotation.x, XMConvertToRadians(35)), "Inspector converts rotation degrees through setter");
	CheckPose(go, Access::Body(rb));
	enterValue(vectorFieldId("Scale"), "2");
	Check(Near(go.GetTransform().scale.x, 2), "Inspector scale edit uses setter");
	const auto bounds = b3Shape_GetAABB(Access::Shape(collider));
	Check(Near(bounds.upperBound.x - bounds.lowerBound.x, 2, 0.05f), "Inspector scaling immediately rebuilds box geometry");
	ImGui::DestroyContext();
}
}

int main()
{
	try
	{
		const int worlds = b3GetWorldCount();
		{
			auto physics = PhysicsTestAccess::Create();
			TestOwnership(); TestGeometryAndContacts(); TestTransforms(); TestInspector(); TestDebugDrawingComponents();
			Check(b3World_GetCounters(World()).bodyCount == 0, "All scene destructors release physics resources");
		}
		Check(b3GetWorldCount() == worlds, "Shutdown releases world");
		std::cout << "PASS " << checks << " physics component checks\n";
		return 0;
	}
	catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
