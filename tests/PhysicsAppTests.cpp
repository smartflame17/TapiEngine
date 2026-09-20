#include "../App.h"
#include "PhysicsTestAccess.h"
#include "PhysicsComponentTestAccess.h"
#include <box3d/box3d.h>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <ScreenGrab.h>
#include <wincodec.h>
#include <filesystem>
#include <d3d11sdklayers.h>
#include "../imgui/imgui_internal.h"

class ImguiSettingsTestAccess
{
public:
	static void Open(ImguiManager& ui, bool open) { ui.settingsWindowOpen = open; }
	static PhysicsDebugDrawSettings* Settings(ImguiManager& ui) { return ui.context.physicsDebugSettings; }
};

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
	static Graphics& Gfx(App& app) { return app.wnd.Gfx(); }
	static ImguiManager& Ui(App& app) { return app.imgui; }
	static void DebugDraw(App& app) { app.DrawPhysicsDebug(); }
	static void DebugCamera(App& app)
	{
		app.editorCam.SetPosition(300, 0, 0);
		app.editorCam.SetRotation(0, 0, 0);
		app.activeCam = &app.editorCam;
		app.wnd.Gfx().SetCamera(app.editorCam.GetViewMatrix());
	}
	static void ResetCamera(App& app) { app.editorCam.Reset(); }
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

struct GraphicsAccess : IBindable
{
	using IBindable::GetContext;
	using IBindable::GetDevice;
	void Bind(Graphics&) noexcept override {}
};

// Read the real D3D render target, including the line shader and render states.
std::size_t ColorPixels(Graphics& gfx, unsigned char red, unsigned char green, unsigned char blue, const wchar_t* capture = nullptr)
{
	using Microsoft::WRL::ComPtr;
	auto* context = GraphicsAccess::GetContext(gfx);
	ComPtr<ID3D11RenderTargetView> target;
	context->OMGetRenderTargets(1, &target, nullptr);
	ComPtr<ID3D11Resource> resource;
	target->GetResource(&resource);
	ComPtr<ID3D11Texture2D> texture;
	resource.As(&texture);
	if (capture)
	{
#ifdef _DEBUG
		const auto path = std::filesystem::path("x64/PhysicsTests/Debug") / capture;
#else
		const auto path = std::filesystem::path("x64/PhysicsTests/Release") / capture;
#endif
		Check(SUCCEEDED(DirectX::SaveWICTextureToFile(context, texture.Get(), GUID_ContainerFormatPng, path.c_str())), "Save collider render capture");
	}
	D3D11_TEXTURE2D_DESC desc;
	texture->GetDesc(&desc);
	const bool bgra = desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM;
	Check(bgra || desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM, "Known render-target pixel format");
	desc.Usage = D3D11_USAGE_STAGING;
	desc.BindFlags = desc.MiscFlags = 0;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	ComPtr<ID3D11Texture2D> staging;
	Check(SUCCEEDED(GraphicsAccess::GetDevice(gfx)->CreateTexture2D(&desc, nullptr, &staging)), "Create debug readback texture");
	context->CopyResource(staging.Get(), texture.Get());
	D3D11_MAPPED_SUBRESOURCE mapped;
	Check(SUCCEEDED(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped)), "Read collider pixels");
	std::size_t count = 0;
	for (UINT y = 0; y < desc.Height; ++y)
		for (UINT x = 0; x < desc.Width; ++x)
		{
			const auto* pixel = static_cast<const unsigned char*>(mapped.pData) + y * mapped.RowPitch + x * 4;
			if (std::abs(int(pixel[bgra ? 2 : 0]) - red) <= 1 && std::abs(int(pixel[1]) - green) <= 1 &&
				std::abs(int(pixel[bgra ? 0 : 2]) - blue) <= 1) ++count;
		}
	context->Unmap(staging.Get(), 0);
	return count;
}

void TestDebugRendering(App& app)
{
	using Microsoft::WRL::ComPtr;
	auto& gfx = AppPhysicsTestAccess::Gfx(app);
	auto& scene = AppPhysicsTestAccess::SceneOf(app);
	auto& settings = Physics::GetInstance().GetDebugDrawSettings();
	const auto projection = gfx.GetProjection();
	AppPhysicsTestAccess::DebugCamera(app);
	gfx.SetProjection(DirectX::XMMatrixPerspectiveFovLH(1.0f, gfx.GetViewportWidth() / gfx.GetViewportHeight(), 0.1f, 100));
	gfx.DisableImGui();
	auto& first = scene.CreateGameObject("Debug render fixture");
	first.SetPosition(300, 0, 5);
	auto& collider = first.AddComponent<Collider>();
	ComPtr<ID3D11InfoQueue> messages;
	GraphicsAccess::GetDevice(gfx)->QueryInterface(IID_PPV_ARGS(&messages));
	if (messages) messages->ClearStoredMessages();
	const auto begin = [&]() {
		gfx.BeginFrame(0.1f, 0.1f, 0.1f);
		gfx.RestoreDefaultStates();
	};
	begin();
	AppPhysicsTestAccess::DebugDraw(app);
	Check(ColorPixels(gfx, 0, 0, 255, L"collider-idle.png") > 100, "Edit collider is blue even beyond Box3D default draw bounds");
	Check(!gfx.GetWireframeDebugSettings().enabled, "Collider visibility is independent of BVH debug setting");
	const auto occlude = [&]() {
		ComPtr<ID3D11DepthStencilView> depth;
		GraphicsAccess::GetContext(gfx)->OMGetRenderTargets(0, nullptr, &depth);
		GraphicsAccess::GetContext(gfx)->ClearDepthStencilView(depth.Get(), D3D11_CLEAR_DEPTH, 0, 0);
	};
	begin(); occlude();
	AppPhysicsTestAccess::DebugDraw(app);
	Check(ColorPixels(gfx, 0, 0, 255, L"collider-occluded.png") > 100, "Collider lines remain visible behind a fully occluding depth buffer");
	ComPtr<ID3D11DepthStencilState> depthState;
	GraphicsAccess::GetContext(gfx)->OMGetDepthStencilState(&depthState, nullptr);
	Check(depthState.Get() == gfx.GetDepthStencilState(), "Collider rendering restores default depth state");
	begin(); occlude();
	gfx.DrawWireframeBoundingBox({ { 300, 0, 5 }, { 0.5f, 0.5f, 0.5f } });
	Check(ColorPixels(gfx, 0, 255, 0) == 0, "Existing BVH wireframes still respect depth occlusion");
	begin();
	gfx.DrawWireframeBoundingBox({ { 300, 0, 5 }, { 0.5f, 0.5f, 0.5f } });
	Check(ColorPixels(gfx, 0, 255, 0) > 100, "Existing BVH wireframes still draw with clear depth");
	AppPhysicsTestAccess::Mode(app, true, false);
	begin(); AppPhysicsTestAccess::DebugDraw(app);
	Check(ColorPixels(gfx, 0, 0, 255) == 0, "Play hides colliders by default");
	settings.drawDuringPlay = true;
	begin(); AppPhysicsTestAccess::DebugDraw(app);
	Check(ColorPixels(gfx, 0, 0, 255) > 100, "Play setting enables collider drawing");
	AppPhysicsTestAccess::Mode(app, true, true);
	begin(); AppPhysicsTestAccess::DebugDraw(app);
	Check(ColorPixels(gfx, 0, 0, 255) > 100, "Paused Play honors the enabled setting");
	settings.drawDuringPlay = false;
	begin(); AppPhysicsTestAccess::DebugDraw(app);
	Check(ColorPixels(gfx, 0, 0, 255) == 0, "Paused Play honors the disabled setting");
	AppPhysicsTestAccess::Mode(app, false, false);
	settings.idleColor = { 0, 1, 1 };
	begin(); AppPhysicsTestAccess::DebugDraw(app);
	Check(ColorPixels(gfx, 0, 255, 255) > 100, "Idle color change reaches the next frame");
	auto& second = scene.CreateGameObject("Overlapping debug fixture");
	second.SetPosition(300.25f, 0, 5);
	second.AddComponent<Collider>();
	begin(); AppPhysicsTestAccess::DebugDraw(app);
	Check(ColorPixels(gfx, 255, 0, 0, L"collider-collision.png") > 100 && ColorPixels(gfx, 0, 255, 255) == 0, "Edit overlaps render both colliders red");
	settings.collisionColor = { 1, 0, 1 };
	begin(); AppPhysicsTestAccess::DebugDraw(app);
	Check(ColorPixels(gfx, 255, 0, 255) > 100, "Collision color change reaches the next frame");
	second.Destroy(); scene.CleanupDestroyedObjects();
	settings = {};
	for (const auto shape : { Collider::Shape::Sphere, Collider::Shape::Capsule })
	{
		auto geometry = collider.GetGeometry(); geometry.shape = shape; collider.SetGeometry(geometry);
		begin(); AppPhysicsTestAccess::DebugDraw(app);
		Check(ColorPixels(gfx, 0, 0, 255, shape == Collider::Shape::Sphere ? L"collider-sphere.png" : L"collider-capsule.png") > 100, "Curved collider wireframes reach the GPU");
	}
	// Force line-buffer growth, then render a smaller frame through the same allocation.
	std::vector<DirectX::XMFLOAT3> many(8192);
	for (std::size_t i = 0; i < many.size(); ++i) many[i] = { 300 + (i % 2 ? 0.5f : -0.5f), 0, 5 };
	begin(); gfx.DrawWireframeLines(many, { 1, 1, 0 }, false);
	Check(ColorPixels(gfx, 255, 255, 0) > 100, "Dynamic line buffer grows for larger batches");
	begin(); AppPhysicsTestAccess::DebugDraw(app);
	Check(ColorPixels(gfx, 0, 0, 255) > 100, "Smaller batches reuse the grown line buffer");
	if (messages)
	{
		for (UINT64 i = 0; i < messages->GetNumStoredMessages(); ++i)
		{
			SIZE_T size = 0; messages->GetMessage(i, nullptr, &size);
			std::vector<unsigned char> storage(size);
			auto* message = reinterpret_cast<D3D11_MESSAGE*>(storage.data());
			messages->GetMessage(i, message, &size);
			Check(message->Severity != D3D11_MESSAGE_SEVERITY_CORRUPTION && message->Severity != D3D11_MESSAGE_SEVERITY_ERROR,
				"D3D debug layer reports no collider rendering errors");
		}
	}
	first.Destroy(); scene.CleanupDestroyedObjects();
	Check(PhysicsTestAccess::DebugShapeCount(Physics::GetInstance()) == 0, "GPU fixture removal releases CPU debug caches");
	gfx.SetProjection(projection);
	gfx.EnableImGui();
	AppPhysicsTestAccess::ResetCamera(app);
}

void TestSettings(App& app)
{
	auto& ui = AppPhysicsTestAccess::Ui(app);
	auto& settings = Physics::GetInstance().GetDebugDrawSettings();
	ImguiSettingsTestAccess::Open(ui, true);
	AppPhysicsTestAccess::Render(app);
	AppPhysicsTestAccess::Render(app);
	Check(ImguiSettingsTestAccess::Settings(ui) == &settings, "Settings window edits the App-owned Physics preferences");
	auto* window = ImGui::FindWindowByName("Settings");
	Check(window != nullptr, "Settings window opens");
	auto* tabs = ImGui::GetCurrentContext()->TabBars.GetByKey(ImHashStr("SettingsTabs", 0, window->ID));
	Check(tabs && tabs->Tabs.Size == 2, "Settings separates General and Physics tabs");
	const auto general = ImHashStr("General", 0, tabs->ID);
	const auto physics = ImHashStr("Physics", 0, tabs->ID);
	tabs->NextSelectedTabId = physics;
	AppPhysicsTestAccess::Render(app);
	Check(tabs->SelectedTabId == physics, "Physics tab can be selected");
	ImGui::ActivateItemByID(ImHashStr("Draw Colliders During Play", 0, physics));
	AppPhysicsTestAccess::Render(app);
	Check(settings.drawDuringPlay, "Physics checkbox updates the live Play visibility setting");
	ColorPixels(AppPhysicsTestAccess::Gfx(app), 0, 0, 255, L"physics-settings.png");
	tabs->NextSelectedTabId = general;
	AppPhysicsTestAccess::Render(app);
	ImGui::ActivateItemByID(ImHashStr("Draw BVH Wireframes", 0, general));
	AppPhysicsTestAccess::Render(app);
	Check(AppPhysicsTestAccess::Gfx(app).GetWireframeDebugSettings().enabled, "General tab preserves the existing BVH toggle");
	AppPhysicsTestAccess::Gfx(app).GetWireframeDebugSettings().enabled = false;
	settings = {};
	ImguiSettingsTestAccess::Open(ui, false);
}

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
		TestDebugRendering(app);
		TestSettings(app);
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
		auto& debugSettings = Physics::GetInstance().GetDebugDrawSettings();
		debugSettings.drawDuringPlay = true;
		debugSettings.idleColor = { 0.2f, 0.3f, 0.4f };
		Physics::GetInstance().CollectDebugDraw(false, { { -100, -100, -100 }, { 100, 100, 100 } });
		Check(PhysicsTestAccess::DebugShapeCount(Physics::GetInstance()) != 0, "Stop fixture owns live debug caches");
		AppPhysicsTestAccess::Stop(app);
		AppPhysicsTestAccess::Frame(app, 10.0f);
		Check(resetDestroyedWithWorld && !b3World_IsValid(stoppedWorld), "Stop clears components before replacing their world");
		Check(!b3Body_IsValid(simulatedBody), "Stop releases real Rigidbody and Collider resources");
		Check(PhysicsTestAccess::DebugShapeCount(Physics::GetInstance()) == 0 && debugSettings.drawDuringPlay && debugSettings.idleColor.x == 0.2f,
			"Stop releases debug caches and preserves session preferences");
		Check(AppPhysicsTestAccess::IsEditing(app) && AppPhysicsTestAccess::Alpha(app) == 0,
			"Stop returns to Edit mode with no loading-time debt");
		Check(b3GetWorldCount() == baseline + 1 && b3World_GetCounters(World()).bodyCount == 0, "Stop restores an empty world without leaks");
		AppPhysicsTestAccess::Render(app);

		AppPhysicsTestAccess::Mode(app, true, false);
		AppPhysicsTestAccess::Frame(app, 0);
		auto& escapeObject = scene.CreateGameObject("Escape fixture");
		escapeObject.AddComponent<BodyOwner>(escapeDestroyedWithWorld);
		Physics::GetInstance().CollectDebugDraw(true, { { -100, -100, -100 }, { 100, 100, 100 } });
		Check(PhysicsTestAccess::DebugShapeCount(Physics::GetInstance()) != 0, "Escape fixture owns live debug caches");
		const auto escapedWorld = World();
		ImGui::GetIO().WantCaptureKeyboard = false;
		SendMessageA(window, WM_KEYDOWN, VK_ESCAPE, 0);
		AppPhysicsTestAccess::Input(app);
		SendMessageA(window, WM_KEYUP, VK_ESCAPE, 0);
		AppPhysicsTestAccess::Frame(app, 2.0f);
		Check(AppPhysicsTestAccess::IsEditing(app) && escapeDestroyedWithWorld && !b3World_IsValid(escapedWorld),
			"Escape uses the same safe reset as Stop");
		Check(AppPhysicsTestAccess::Alpha(app) == 0, "Escape discards pre-reset frame time");
		Check(PhysicsTestAccess::DebugShapeCount(Physics::GetInstance()) == 0 && debugSettings.drawDuringPlay, "Escape releases debug caches and preserves preferences");
		scene.CreateGameObject("Shutdown fixture").AddComponent<BodyOwner>(shutdownDestroyedWithWorld);
		auto& shutdownObject = scene.CreateGameObject("Component shutdown fixture");
		shutdownObject.AddComponent<Collider>(); shutdownObject.AddComponent<Rigidbody>();
		Physics::GetInstance().CollectDebugDraw(false, { { -100, -100, -100 }, { 100, 100, 100 } });
		Check(PhysicsTestAccess::DebugShapeCount(Physics::GetInstance()) != 0, "App shutdown exercises live debug resources");
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
