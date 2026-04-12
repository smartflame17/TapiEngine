#include "ScriptRegistry.h"
#include "../Components/CustomBehaviour.h"

CustomBehaviour* ScriptRegistry::Create(const std::string& name, GameObject* owner) const noexcept
{
	const auto it = registry.find(name);
	if (it == registry.end())
	{
		return nullptr;
	}

	CustomBehaviour* script = it->second.factory(owner);
	if (script != nullptr)
	{
		script->ConfigureLifecycle(it->second.lifecycleMask);
	}
	return script;
}

bool ScriptRegistry::IsRegistered(const std::string& name) const noexcept
{
	return registry.find(name) != registry.end();
}
