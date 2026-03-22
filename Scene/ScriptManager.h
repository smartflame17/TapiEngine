#pragma once

#include <list>
#include <vector>

class CustomBehaviour;
class GameObject;

class ScriptManager
{
public:
	void RegisterScript(CustomBehaviour& script) noexcept;
	void HandleEnableStateChanged(CustomBehaviour& script) noexcept;
	void QueueDestroy(GameObject& object) noexcept;

	void ProcessAwakeAndStart() noexcept;
	void FixedUpdate() noexcept;
	void Update(float dt) noexcept;
	void LateUpdate(float dt) noexcept;
	void Cleanup() noexcept;
	void Clear() noexcept;

private:
	void ActivatePendingScripts() noexcept;
	void NotifyEnableIfNeeded(CustomBehaviour& script) noexcept;
	void AddToActiveLists(CustomBehaviour& script) noexcept;
	void RemoveFromActiveLists(CustomBehaviour& script) noexcept;
	bool IsScriptRunnable(const CustomBehaviour& script) const noexcept;
	bool Contains(const std::list<CustomBehaviour*>& scripts, const CustomBehaviour& script) const noexcept;
	bool Contains(const std::vector<CustomBehaviour*>& scripts, const CustomBehaviour& script) const noexcept;

private:
	std::vector<CustomBehaviour*> pendingRegistration;
	std::vector<CustomBehaviour*> pendingImmediateActivation;
	std::vector<CustomBehaviour*> awakeQueue;
	std::vector<CustomBehaviour*> startQueue;
	std::list<CustomBehaviour*> fixedUpdateList;
	std::list<CustomBehaviour*> updateList;
	std::list<CustomBehaviour*> lateUpdateList;
	std::vector<CustomBehaviour*> destroyQueue;
};
