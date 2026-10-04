#include "ScriptRegistry.h"
#include "../Components/CustomBehaviour.h"

CustomBehaviour* ScriptRegistry::Create(const std::string& name, GameObject* owner) const
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

std::string ScriptRegistry::GetRegisteredName(const CustomBehaviour& script) const
{
	const auto it = typeNames.find(std::type_index(typeid(script)));
	return it == typeNames.end() ? std::string{} : it->second;
}

bool ScriptRegistry::IsRegistered(const std::string& name) const noexcept
{
	return registry.find(name) != registry.end();
}
