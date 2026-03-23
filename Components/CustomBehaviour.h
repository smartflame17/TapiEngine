#pragma once

#include "Component.h"
#include <cstdint>

class Scene;

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

private:
	bool isEnabled = true;
	bool hasStarted = false;
	bool enableNotified = false;
	bool queuedForDestroy = false;
	std::uint8_t lifecycleMask = None;
};
