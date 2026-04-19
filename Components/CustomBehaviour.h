#pragma once

#include "Component.h"
#include "../Scene/ScriptRegistry.h"
#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

class Scene;

// Enum to represent different property types for script properties (for inspector display and serialization)
enum class PropertyType
{
	Int,
	Float,
	String,
	Vector3,
	Color,
	Bool
};

struct ExposedProperty
{
	std::string name;
	PropertyType type;
	void* value; // Pointer to actual property value
};

class CustomBehaviour : public Component
{
public:
	enum LifecycleFlags : std::uint8_t
	{
		None = 0,
		AwakeFlag = 1 << 0,
		OnEnableFlag = 1 << 1,
		StartFlag = 1 << 2,
		UpdateFlag = 1 << 3,
		FixedUpdateFlag = 1 << 4,
		LateUpdateFlag = 1 << 5,
		OnDisableFlag = 1 << 6,
		OnDestroyFlag = 1 << 7
	};

public:
	CustomBehaviour() = default;
	virtual ~CustomBehaviour() = default;
	CustomBehaviour(GameObject* owner) : Component()
	{
		SetOwner(owner);
	}

	// Lifecycle methods to be overridden by derived classes
	virtual void Awake();
	virtual void OnEnable();
	virtual void Start();
	virtual void Update(float dt);
	virtual void FixedUpdate();
	virtual void LateUpdate(float dt);
	virtual void OnDisable();
	virtual void OnDestroy();

	bool IsEnabled() const noexcept;
	void SetEnabled(bool enabled) noexcept;
	bool HasStarted() const noexcept;

	Scene& GetScene() const noexcept;

	// Helper methods for lifecycle management
	bool SupportsAwake() const noexcept;
	bool SupportsOnEnable() const noexcept;
	bool SupportsStart() const noexcept;
	bool SupportsUpdate() const noexcept;
	bool SupportsFixedUpdate() const noexcept;
	bool SupportsLateUpdate() const noexcept;
	bool SupportsOnDisable() const noexcept;
	bool SupportsOnDestroy() const noexcept;

	void ConfigureLifecycle(std::uint8_t mask) noexcept;

	void MarkStarted() noexcept;
	void MarkEnableNotified(bool notified) noexcept;
	bool WasEnableNotified() const noexcept;
	void MarkQueuedForDestroy(bool queued) noexcept;
	bool IsQueuedForDestroy() const noexcept;

	// Methods for inspector display and serialization
	virtual void ExposeVariables() {} // Override by derived classes to expose variables for inspector and serialization
	void SetScriptName(const std::string& name) noexcept { scriptName = name; }
	const std::string& GetScriptName() const noexcept { return scriptName; }

protected:
	void ExposeInt(const std::string& name, int* value) {
		properties.push_back({ name, PropertyType::Int, value });
	}
	void ExposeFloat(const std::string& name, float* value) {
		properties.push_back({ name, PropertyType::Float, value });
	}
	void ExposeString(const std::string& name, std::string* value) {
		properties.push_back({ name, PropertyType::String, value });
	}
	void ExposeVector3(const std::string& name, float* value) {
		properties.push_back({ name, PropertyType::Vector3, value });
	}
	void ExposeColor(const std::string& name, float* value) {
		properties.push_back({ name, PropertyType::Color, value });
	}
	void ExposeBool(const std::string& name, bool* value) {
		properties.push_back({ name, PropertyType::Bool, value });
	}
public:
	std::vector<ExposedProperty> properties; // List of exposed properties for inspector and serialization
private:
	const char* GetInspectorTitle() const noexcept override;
	void DrawInspectorContents() noexcept override;

private:
	bool isEnabled = true;
	bool hasStarted = false;
	bool enableNotified = false;
	bool queuedForDestroy = false;
	std::uint8_t lifecycleMask = None;
	std::string scriptName; // For inspector display
};

template<typename T>
constexpr std::uint8_t BuildScriptLifecycleMask() noexcept
{
	static_assert(std::is_base_of_v<CustomBehaviour, T>, "T must derive from CustomBehaviour");

	std::uint8_t mask = CustomBehaviour::None;

	if constexpr (static_cast<void (CustomBehaviour::*)()>(&T::Awake) != &CustomBehaviour::Awake)
		mask |= CustomBehaviour::AwakeFlag;
	if constexpr (static_cast<void (CustomBehaviour::*)()>(&T::OnEnable) != &CustomBehaviour::OnEnable)
		mask |= CustomBehaviour::OnEnableFlag;
	if constexpr (static_cast<void (CustomBehaviour::*)()>(&T::Start) != &CustomBehaviour::Start)
		mask |= CustomBehaviour::StartFlag;
	if constexpr (static_cast<void (CustomBehaviour::*)(float)>(&T::Update) != &CustomBehaviour::Update)
		mask |= CustomBehaviour::UpdateFlag;
	if constexpr (static_cast<void (CustomBehaviour::*)()>(&T::FixedUpdate) != &CustomBehaviour::FixedUpdate)
		mask |= CustomBehaviour::FixedUpdateFlag;
	if constexpr (static_cast<void (CustomBehaviour::*)(float)>(&T::LateUpdate) != &CustomBehaviour::LateUpdate)
		mask |= CustomBehaviour::LateUpdateFlag;
	if constexpr (static_cast<void (CustomBehaviour::*)()>(&T::OnDisable) != &CustomBehaviour::OnDisable)
		mask |= CustomBehaviour::OnDisableFlag;
	if constexpr (static_cast<void (CustomBehaviour::*)()>(&T::OnDestroy) != &CustomBehaviour::OnDestroy)
		mask |= CustomBehaviour::OnDestroyFlag;

	return mask;
}

// Macro to register a script class with the ScriptRegistry, insert to end of cpp file
#define REGISTER_SCRIPT(ScriptClass) \
    namespace { \
        const bool ScriptClass##_registered = []() { \
            ScriptRegistry::GetInstance().Register<ScriptClass>(#ScriptClass, BuildScriptLifecycleMask<ScriptClass>()); \
            return true; \
        }(); \
    }
