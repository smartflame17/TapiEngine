#include "ScriptManager.h"
#include "../Components/CustomBehaviour.h"
#include "GameObject.h"
#include <algorithm>

void ScriptManager::RegisterScript(CustomBehaviour& script) noexcept
{
	if (registeredScripts.insert(&script).second)
	{
		pendingRegistration.push_back(&script);
	}
}

void ScriptManager::UnregisterScript(CustomBehaviour& script) noexcept
{
	registeredScripts.erase(&script);
	RemoveFromActiveLists(script);
	auto removePending = [&](auto& scripts)
	{
		scripts.erase(std::remove(scripts.begin(), scripts.end(), &script), scripts.end());
	};
	removePending(pendingRegistration);
	removePending(pendingEnableChanges);
	removePending(pendingImmediateActivation);
	removePending(awakeQueue);
	removePending(startQueue);
	removePending(destroyQueue);

	if (script.WasEnableNotified() && script.SupportsOnDisable())
	{
		script.OnDisable();
	}

	if (script.SupportsOnDestroy())
	{
		script.OnDestroy();
	}

	script.MarkEnableNotified(false);
	script.MarkQueuedForDestroy(false);
}

void ScriptManager::HandleEnableStateChanged(CustomBehaviour& script) noexcept
{
	// Constructors may set enabled state before ownership/registration is complete.
	if (!registeredScripts.contains(&script) || script.GetGameObject().IsPendingKill()) return;
	if (!script.IsEnabled()) RemoveFromActiveLists(script);
	if (!Contains(pendingEnableChanges, script)) pendingEnableChanges.push_back(&script);
}

void ScriptManager::QueueDestroy(GameObject& object) noexcept
{
	object.ForEachScript([this](CustomBehaviour& script)
	{
		if (script.IsQueuedForDestroy())
		{
			return;
		}

		script.MarkQueuedForDestroy(true);
		RemoveFromActiveLists(script);

		auto removeQueued = [&](auto& scripts)
		{
			scripts.erase(std::remove(scripts.begin(), scripts.end(), &script), scripts.end());
		};

		removeQueued(pendingRegistration);
		removeQueued(pendingEnableChanges);
		removeQueued(pendingImmediateActivation);
		removeQueued(awakeQueue);
		removeQueued(startQueue);

		destroyQueue.push_back(&script);
	});
}

void ScriptManager::ProcessAwakeAndStart() noexcept
{
	auto changes = std::move(pendingEnableChanges);
	pendingEnableChanges.clear();
	for (auto* script : changes)
	{
		if (!script || script->GetGameObject().IsPendingKill()) continue;
		if (!script->IsEnabled())
		{
			if (script->WasEnableNotified() && script->SupportsOnDisable()) script->OnDisable();
			script->MarkEnableNotified(false);
		}
		else if (script->HasStarted()) { NotifyEnableIfNeeded(*script); AddToActiveLists(*script); }
	}
	ActivatePendingScripts();

	auto awake = std::move(awakeQueue); awakeQueue.clear();
	for (CustomBehaviour* script : awake)
	{
		if (script == nullptr || !IsScriptRunnable(*script))
		{
			if (script && !script->GetGameObject().IsPendingKill()) awakeQueue.push_back(script);
			continue;
		}

		script->Awake();
		NotifyEnableIfNeeded(*script);
	}

	auto immediate = std::move(pendingImmediateActivation); pendingImmediateActivation.clear();
	for (CustomBehaviour* script : immediate)
	{
		if (script == nullptr || !IsScriptRunnable(*script))
		{
			if (script && !script->GetGameObject().IsPendingKill()) pendingImmediateActivation.push_back(script);
			continue;
		}

		NotifyEnableIfNeeded(*script);
		script->MarkStarted();
		AddToActiveLists(*script);
	}

	auto starts = std::move(startQueue); startQueue.clear();
	for (CustomBehaviour* script : starts)
	{
		if (script == nullptr || !IsScriptRunnable(*script))
		{
			if (script && !script->GetGameObject().IsPendingKill()) startQueue.push_back(script);
			continue;
		}

		NotifyEnableIfNeeded(*script);
		if (!IsScriptRunnable(*script)) { startQueue.push_back(script); continue; }
		script->Start();
		script->MarkStarted();
		AddToActiveLists(*script);
	}
}

void ScriptManager::FixedUpdate() noexcept
{
	const std::vector<CustomBehaviour*> scripts(fixedUpdateList.begin(), fixedUpdateList.end());
	for (auto* script : scripts)
	{
		if (script == nullptr || !IsScriptRunnable(*script))
		{
			continue;
		}

		script->FixedUpdate();
	}
}

void ScriptManager::Update(float dt) noexcept
{
	const std::vector<CustomBehaviour*> scripts(updateList.begin(), updateList.end());
	for (auto* script : scripts)
	{
		if (script == nullptr || !IsScriptRunnable(*script))
		{
			continue;
		}

		script->Update(dt);
	}
}

void ScriptManager::LateUpdate(float dt) noexcept
{
	const std::vector<CustomBehaviour*> scripts(lateUpdateList.begin(), lateUpdateList.end());
	for (auto* script : scripts)
	{
		if (script == nullptr || !IsScriptRunnable(*script))
		{
			continue;
		}

		script->LateUpdate(dt);
	}
}

void ScriptManager::Cleanup() noexcept
{
	for (CustomBehaviour* script : destroyQueue)
	{
		if (script == nullptr)
		{
			continue;
		}

		if (script->SupportsOnDestroy())
		{
			script->OnDestroy();
		}

		if (script->WasEnableNotified() && script->SupportsOnDisable())
		{
			script->OnDisable();
		}

		script->MarkEnableNotified(false);
		script->MarkQueuedForDestroy(false);
		registeredScripts.erase(script);
	}
	destroyQueue.clear();
}

void ScriptManager::Clear() noexcept
{
	pendingRegistration.clear();
	registeredScripts.clear();
	pendingEnableChanges.clear();
	pendingImmediateActivation.clear();
	awakeQueue.clear();
	startQueue.clear();
	fixedUpdateList.clear();
	updateList.clear();
	lateUpdateList.clear();
	destroyQueue.clear();
}

void ScriptManager::ActivatePendingScripts() noexcept
{
	auto registrations = std::move(pendingRegistration); pendingRegistration.clear();
	for (CustomBehaviour* script : registrations)
	{
		if (script == nullptr || script->GetGameObject().IsPendingKill())
		{
			continue;
		}
		if (!script->IsEnabled()) { pendingRegistration.push_back(script); continue; }

		if (script->SupportsAwake())
		{
			awakeQueue.push_back(script);
		}
		else if (script->IsEnabled())
		{
			NotifyEnableIfNeeded(*script);
		}

		if (script->SupportsStart())
		{
			startQueue.push_back(script);
		}
		else if (script->IsEnabled())
		{
			pendingImmediateActivation.push_back(script);
		}
	}
}

void ScriptManager::NotifyEnableIfNeeded(CustomBehaviour& script) noexcept
{
	if (!script.IsEnabled() || script.WasEnableNotified())
	{
		return;
	}

	script.MarkEnableNotified(true);
	if (script.SupportsOnEnable()) script.OnEnable();
}

void ScriptManager::AddToActiveLists(CustomBehaviour& script) noexcept
{
	if (!script.IsEnabled() || script.GetGameObject().IsPendingKill())
	{
		return;
	}

	if (script.SupportsFixedUpdate() && !Contains(fixedUpdateList, script))
	{
		fixedUpdateList.push_back(&script);
	}
	if (script.SupportsUpdate() && !Contains(updateList, script))
	{
		updateList.push_back(&script);
	}
	if (script.SupportsLateUpdate() && !Contains(lateUpdateList, script))
	{
		lateUpdateList.push_back(&script);
	}
}

void ScriptManager::RemoveFromActiveLists(CustomBehaviour& script) noexcept
{
	fixedUpdateList.remove(&script);
	updateList.remove(&script);
	lateUpdateList.remove(&script);
}

bool ScriptManager::IsScriptRunnable(const CustomBehaviour& script) const noexcept
{
	const GameObject& owner = script.GetGameObject();
	return !owner.IsPendingKill() && script.IsEnabled();
}

bool ScriptManager::Contains(const std::list<CustomBehaviour*>& scripts, const CustomBehaviour& script) const noexcept
{
	return std::find(scripts.begin(), scripts.end(), &script) != scripts.end();
}

bool ScriptManager::Contains(const std::vector<CustomBehaviour*>& scripts, const CustomBehaviour& script) const noexcept
{
	return std::find(scripts.begin(), scripts.end(), &script) != scripts.end();
}
