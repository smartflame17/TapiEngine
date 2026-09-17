#include "../App.h"
#include "PhysicsTestAccess.h"
#include "PhysicsComponentTestAccess.h"
#include <box3d/box3d.h>
#include <cmath>
#include <iostream>
#include <stdexcept>

class AppPhysicsTestAccess
{
public:
	static void Frame(App& app, float delta) { app.Update(delta); }
	static void Render(App& app) { app.RenderFrame(static_cast<float>(app.fixedClock.GetAlpha())); }
	static void Mode(App& app, bool play, bool paused) { app.isPlayMode = play; app.isPaused = paused; }
	static void Stop(App& app) { Mode(app, false, false); app.needsReset = true; }
	static bool IsEditing(const App& app) { return !app.isPlayMode && !app.isPaused && !app.needsReset; }
	static void Input(App& app) { app.HandleInput(app.dt); }
	static Scene& SceneOf(App& app) { return app.scene; }
	static double Alpha(const App& app) { return app.fixedClock.GetAlpha(); }
	static void ReleaseCursor(App& app) { app.wnd.EnableCursor(); }
};

namespace
{
int checks = 0;
void Check(bool condition, const char* message)
{
	++checks;
	if (!condition) throw std::runtime_error(message);
}

b3WorldId World() { return PhysicsTestAccess::World(Physics::GetInstance()); }

// A test-only component proves that component teardown precedes world teardown.
class BodyOwner : public Component
{
public:
	explicit BodyOwner(bool& destroyedWithLiveWorld) : result(destroyedWithLiveWorld)
	{
		auto definition = b3DefaultBodyDef();
		definition.type = b3_dynamicBody;
		definition.position = { 0, 10, 0 };
		body = b3CreateBody(World(), &definition);
		auto shape = b3DefaultShapeDef();
		auto box = b3MakeBoxHull(0.5f, 0.5f, 0.5f);
		b3CreateHullShape(body, &shape, &box.base);
	}
	~BodyOwner() override
	{
		result = b3World_IsValid(World()) && b3Body_IsValid(body);
		if (result) b3DestroyBody(body);
	}
	b3BodyId body{};
private:
	bool& result;
};

class OrderProbe : public CustomBehaviour
{
public:
	explicit OrderProbe(b3BodyId body, GameObject* object = nullptr) : body(body), object(object) {}
	void Awake() override { awake = true; }
	void Start() override { startedAfterAwake = awake; }
	void FixedUpdate() override
	{
		++ticks;
		fixedAfterStart = fixedAfterStart && startedAfterAwake;
		updated = false;
		b3Body_SetLinearVelocity(body, { 0, 10, 0 });
	}
	void Update(float) override
	{
		const float velocity = b3Body_GetLinearVelocity(body).y;
		updateAfterPhysics = updateAfterPhysics && velocity < 10 && velocity > 9;
		if (object) transformAfterPhysics = transformAfterPhysics &&
			std::abs(object->GetTransform().position.y - b3Body_GetPosition(body).y) < 0.0001f;
		updated = true;
	}
	void LateUpdate(float) override
	{
		lateAfterUpdate = lateAfterUpdate && updated;
		if (object) transformAfterPhysics = transformAfterPhysics &&
			std::abs(object->GetTransform().position.y - b3Body_GetPosition(body).y) < 0.0001f;
	}
	int ticks = 0;
	bool awake = false, startedAfterAwake = false;
	bool fixedAfterStart = true, updateAfterPhysics = true, lateAfterUpdate = true;
	bool transformAfterPhysics = true;
private:
	b3BodyId body;
	GameObject* object;
	bool updated = false;
};

HWND HideTestWindow()
{
	HWND result = nullptr;
	EnumThreadWindows(GetCurrentThreadId(), [](HWND window, LPARAM context) -> BOOL
	{
		char name[128]{};
		GetClassNameA(window, name, sizeof(name));
		if (std::string(name) == "SmartFlame's Dx3D Engine")
		{
			*reinterpret_cast<HWND*>(context) = window;
			ShowWindow(window, SW_HIDE);
			return FALSE;
		}
		return TRUE;
	}, reinterpret_cast<LPARAM>(&result));
	return result;
}

void TestApp()
{
	const int baseline = b3GetWorldCount();
	bool resetDestroyedWithWorld = false;
	bool escapeDestroyedWithWorld = false;
	bool shutdownDestroyedWithWorld = false;
	{
		App app;
		AppPhysicsTestAccess::ReleaseCursor(app);
		const HWND window = HideTestWindow();
		Check(window != nullptr, "App created its test window");
		ImGui::GetIO().IniFilename = nullptr;
		const auto* context = ImGui::GetCurrentContext();
		bool duplicateRejected = false;
		try { App duplicate; }
		catch (const std::logic_error&) { duplicateRejected = true; }
		Check(duplicateRejected && ImGui::GetCurrentContext() == context,
			"A second App is rejected before touching the active ImGui context");
		Check(b3GetWorldCount() == baseline + 1, "App owns one world");
		Check(b3World_GetCounters(World()).bodyCount == 0, "Production scene starts with an empty world");
		auto& scene = AppPhysicsTestAccess::SceneOf(app);
		auto& object = scene.CreateGameObject("Physics lifecycle fixture");
		auto& owner = object.AddComponent<BodyOwner>(resetDestroyedWithWorld);
		const auto body = owner.body;
		auto& probe = object.AddComponent<OrderProbe>(body);
		auto& simulated = scene.CreateGameObject("Component synchronization fixture");
		simulated.SetPosition(5, 10, 0);
		auto& rigidbody = simulated.AddComponent<Rigidbody>();
		simulated.AddComponent<Collider>();
		rigidbody.SetType(Rigidbody::Type::Dynamic);
		const auto simulatedBody = PhysicsComponentTestAccess::Body(rigidbody);
		auto& componentProbe = simulated.AddComponent<OrderProbe>(simulatedBody, &simulated);
		AppPhysicsTestAccess::Frame(app, 5.0f);
		Check(b3Body_GetPosition(body).y == 10 && probe.ticks == 0, "Edit mode does not simulate physics or scripts");

		AppPhysicsTestAccess::Mode(app, true, false);
		AppPhysicsTestAccess::Frame(app, 5.0f);
		Check(probe.ticks == 0 && AppPhysicsTestAccess::Alpha(app) == 0, "Play discards time crossing the mode boundary");
		AppPhysicsTestAccess::Frame(app, 1.0f / 30.0f);
		Check(probe.ticks == 2, "A 30 Hz render interval produces two fixed ticks");
		Check(probe.fixedAfterStart && probe.updateAfterPhysics && probe.lateAfterUpdate,
			"Lifecycle order is Awake/Start, FixedUpdate, physics, Update, LateUpdate");
		Check(componentProbe.ticks == 2 && componentProbe.transformAfterPhysics,
			"Current physics pose reaches script Update and LateUpdate in the same fixed tick");
		AppPhysicsTestAccess::Frame(app, 1.0f / 240.0f);
		Check(AppPhysicsTestAccess::Alpha(app) > 0, "Running mode retains fractional time");
		AppPhysicsTestAccess::Mode(app, true, true);
		AppPhysicsTestAccess::Frame(app, 1.0f);
		Check(AppPhysicsTestAccess::Alpha(app) == 0, "Pause clears the previous partial tick");
		const auto pausedPosition = b3Body_GetPosition(body);
		const auto pausedTransform = simulated.GetTransform();
		AppPhysicsTestAccess::Frame(app, 5.0f);
		AppPhysicsTestAccess::Render(app);
		Check(b3Body_GetPosition(body).y == pausedPosition.y && probe.ticks == 2, "Paused world stays fixed while rendering continues");
		Check(simulated.GetTransform().position.y == pausedTransform.position.y && componentProbe.ticks == 2,
			"Component transforms and script ticks stay stable while paused");
		AppPhysicsTestAccess::Mode(app, true, false);
		AppPhysicsTestAccess::Frame(app, 5.0f);
		Check(probe.ticks == 2 && AppPhysicsTestAccess::Alpha(app) == 0, "Resume discards paused-time debt");
		AppPhysicsTestAccess::Frame(app, 1.0f / 60.0f);
		Check(probe.ticks == 3, "Resume advances on the next fixed interval");
		AppPhysicsTestAccess::Frame(app, 10.0f);
		Check(probe.ticks == 18, "App limits long stalls to fifteen catch-up ticks");
		AppPhysicsTestAccess::Render(app);

		const auto stoppedWorld = World();
		AppPhysicsTestAccess::Stop(app);
		AppPhysicsTestAccess::Frame(app, 10.0f);
		Check(resetDestroyedWithWorld && !b3World_IsValid(stoppedWorld), "Stop clears components before replacing their world");
		Check(!b3Body_IsValid(simulatedBody), "Stop releases real Rigidbody and Collider resources");
		Check(AppPhysicsTestAccess::IsEditing(app) && AppPhysicsTestAccess::Alpha(app) == 0,
			"Stop returns to Edit mode with no loading-time debt");
		Check(b3GetWorldCount() == baseline + 1 && b3World_GetCounters(World()).bodyCount == 0, "Stop restores an empty world without leaks");
		AppPhysicsTestAccess::Render(app);

		AppPhysicsTestAccess::Mode(app, true, false);
		AppPhysicsTestAccess::Frame(app, 0);
		auto& escapeObject = scene.CreateGameObject("Escape fixture");
		escapeObject.AddComponent<BodyOwner>(escapeDestroyedWithWorld);
		const auto escapedWorld = World();
		ImGui::GetIO().WantCaptureKeyboard = false;
		SendMessageA(window, WM_KEYDOWN, VK_ESCAPE, 0);
		AppPhysicsTestAccess::Input(app);
		SendMessageA(window, WM_KEYUP, VK_ESCAPE, 0);
		AppPhysicsTestAccess::Frame(app, 2.0f);
		Check(AppPhysicsTestAccess::IsEditing(app) && escapeDestroyedWithWorld && !b3World_IsValid(escapedWorld),
			"Escape uses the same safe reset as Stop");
		Check(AppPhysicsTestAccess::Alpha(app) == 0, "Escape discards pre-reset frame time");
		scene.CreateGameObject("Shutdown fixture").AddComponent<BodyOwner>(shutdownDestroyedWithWorld);
		auto& shutdownObject = scene.CreateGameObject("Component shutdown fixture");
		shutdownObject.AddComponent<Collider>(); shutdownObject.AddComponent<Rigidbody>();
	}
	Check(shutdownDestroyedWithWorld, "Scene components can access Physics during App shutdown");
	Check(b3GetWorldCount() == baseline, "App shutdown releases its world");
	bool unavailable = false;
	try { Physics::GetInstance(); }
	catch (const std::logic_error&) { unavailable = true; }
	Check(unavailable, "App shutdown unregisters the singleton");
}
}

int main()
{
	try
	{
		Check(SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)), "Initialize COM for default-scene WIC textures");
		struct ComScope { ~ComScope() { CoUninitialize(); } } comScope;
		TestApp();
		std::cout << "PASS " << checks << " App physics checks\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		ClipCursor(nullptr);
		std::cerr << "FAIL: " << error.what() << '\n';
		return 1;
	}
}
