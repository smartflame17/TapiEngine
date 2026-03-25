#include "ScriptManager.h"
#include "../Components/CustomBehaviour.h"
#include "GameObject.h"
#include <algorithm>

void ScriptManager::RegisterScript(CustomBehaviour& script) noexcept
{
	if (!Contains(pendingRegistration, script))
	{
		pendingRegistration.push_back(&script);
	}
}

void ScriptManager::HandleEnableStateChanged(CustomBehaviour& script) noexcept
{
	// Early exit for invalid scipts
	if (script.GetGameObject().IsPendingKill())
	{
		return;
	}

	if (!script.IsEnabled())
	{
		RemoveFromActiveLists(script);
		if (script.WasEnableNotified() && script.SupportsOnDisable())
		{
			script.OnDisable();
		}
		script.MarkEnableNotified(false);
		return;
	}

	NotifyEnableIfNeeded(script);
	if (script.HasStarted())
	{
		AddToActiveLists(script);
	}
	else if (!Contains(pendingImmediateActivation, script) && !script.SupportsStart())
	{
		pendingImmediateActivation.push_back(&script);
	}
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
		removeQueued(pendingImmediateActivation);
		removeQueued(awakeQueue);
		removeQueued(startQueue);

		destroyQueue.push_back(&script);
	});
}

void ScriptManager::ProcessAwakeAndStart() noexcept
{
	ActivatePendingScripts();

	for (CustomBehaviour* script : awakeQueue)
	{
		if (script == nullptr || !IsScriptRunnable(*script))
		{
			continue;
		}

		script->Awake();
		NotifyEnableIfNeeded(*script);
	}
	awakeQueue.clear();

	for (CustomBehaviour* script : pendingImmediateActivation)
	{
		if (script == nullptr || !IsScriptRunnable(*script))
		{
			continue;
		}

		NotifyEnableIfNeeded(*script);
		AddToActiveLists(*script);
	}
	pendingImmediateActivation.clear();

	for (CustomBehaviour* script : startQueue)
	{
		if (script == nullptr || !IsScriptRunnable(*script))
		{
			continue;
		}

		NotifyEnableIfNeeded(*script);
		script->Start();
		script->MarkStarted();
		AddToActiveLists(*script);
	}
	startQueue.clear();
}

void ScriptManager::FixedUpdate() noexcept
{
	for (auto it = fixedUpdateList.begin(); it != fixedUpdateList.end(); ++it)
	{
		CustomBehaviour* script = *it;
		if (script == nullptr || !IsScriptRunnable(*script))
		{
			continue;
		}

		script->FixedUpdate();
	}
}

void ScriptManager::Update(float dt) noexcept
{
	for (auto it = updateList.begin(); it != updateList.end(); ++it)
	{
		CustomBehaviour* script = *it;
		if (script == nullptr || !IsScriptRunnable(*script))
		{
			continue;
		}

		script->Update(dt);
	}
}

void ScriptManager::LateUpdate(float dt) noexcept
{
	for (auto it = lateUpdateList.begin(); it != lateUpdateList.end(); ++it)
	{
		CustomBehaviour* script = *it;
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
	}
	destroyQueue.clear();
}

void ScriptManager::Clear() noexcept
{
	pendingRegistration.clear();
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
	for (CustomBehaviour* script : pendingRegistration)
	{
		if (script == nullptr || script->GetGameObject().IsPendingKill())
		{
			continue;
		}

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
	pendingRegistration.clear();
}

void ScriptManager::NotifyEnableIfNeeded(CustomBehaviour& script) noexcept
{
	if (!script.IsEnabled() || script.WasEnableNotified() || !script.SupportsOnEnable())
	{
		return;
	}

	script.OnEnable();
	script.MarkEnableNotified(true);
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
