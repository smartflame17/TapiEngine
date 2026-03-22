#include "CustomBehaviour.h"
#include "../Scene/GameObject.h"
#include "../Scene/Scene.h"

void CustomBehaviour::Awake()
{
}

void CustomBehaviour::OnEnable()
{
}

void CustomBehaviour::Start()
{
}

void CustomBehaviour::Update(float dt)
{
}

void CustomBehaviour::FixedUpdate()
{
}

void CustomBehaviour::LateUpdate(float dt)
{
}

void CustomBehaviour::OnDisable()
{
}

void CustomBehaviour::OnDestroy()
{
}

bool CustomBehaviour::IsEnabled() const noexcept
{
	return isEnabled;
}

void CustomBehaviour::SetEnabled(bool enabled) noexcept
{
	if (isEnabled == enabled)
	{
		return;
	}

	isEnabled = enabled;

	if (auto* owner = TryGetGameObject())
	{
		owner->GetScene().HandleScriptEnableStateChanged(*this);
	}
}

bool CustomBehaviour::HasStarted() const noexcept
{
	return hasStarted;
}

Scene& CustomBehaviour::GetScene() const noexcept
{
	return GetGameObject().GetScene();
}

bool CustomBehaviour::SupportsAwake() const noexcept
{
	return (lifecycleMask & AwakeFlag) != 0;
}

bool CustomBehaviour::SupportsOnEnable() const noexcept
{
	return (lifecycleMask & OnEnableFlag) != 0;
}

bool CustomBehaviour::SupportsStart() const noexcept
{
	return (lifecycleMask & StartFlag) != 0;
}

bool CustomBehaviour::SupportsUpdate() const noexcept
{
	return (lifecycleMask & UpdateFlag) != 0;
}

bool CustomBehaviour::SupportsFixedUpdate() const noexcept
{
	return (lifecycleMask & FixedUpdateFlag) != 0;
}

bool CustomBehaviour::SupportsLateUpdate() const noexcept
{
	return (lifecycleMask & LateUpdateFlag) != 0;
}

bool CustomBehaviour::SupportsOnDisable() const noexcept
{
	return (lifecycleMask & OnDisableFlag) != 0;
}

bool CustomBehaviour::SupportsOnDestroy() const noexcept
{
	return (lifecycleMask & OnDestroyFlag) != 0;
}

void CustomBehaviour::ConfigureLifecycle(std::uint8_t mask) noexcept
{
	lifecycleMask = mask;
}

void CustomBehaviour::MarkStarted() noexcept
{
	hasStarted = true;
}

void CustomBehaviour::MarkEnableNotified(bool notified) noexcept
{
	enableNotified = notified;
}

bool CustomBehaviour::WasEnableNotified() const noexcept
{
	return enableNotified;
}

void CustomBehaviour::MarkQueuedForDestroy(bool queued) noexcept
{
	queuedForDestroy = queued;
}

bool CustomBehaviour::IsQueuedForDestroy() const noexcept
{
	return queuedForDestroy;
}
