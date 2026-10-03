#include "ComponentRegistry.h"
#include "JsonRead.h"
#include <stdexcept>

void RegisterBuiltinComponents(ComponentRegistry& registry);

void ComponentRegistry::Register(ComponentRegistration entry)
{
	if (entry.key.empty() || !entry.factory || Find(entry.key))
		throw std::invalid_argument("A component registration requires a unique type key and a factory.");
	if (entry.sceneSupported && !entry.validate)
		throw std::invalid_argument("A scene component registration requires a metadata validator.");
	entries.push_back(std::move(entry));
}

const ComponentRegistration* ComponentRegistry::Find(std::string_view key) const noexcept
{
	for (const auto& entry : entries) if (entry.key == key) return &entry;
	return nullptr;
}

const ComponentRegistration* ComponentRegistry::Find(ComponentType type) const noexcept
{
	for (const auto& entry : entries) if (entry.type == type) return &entry;
	return nullptr;
}

Component& ComponentRegistry::Create(std::string_view key, GameObject& owner, const nlohmann::json& data, LoadContext& context) const
{
	const auto* entry = Find(key);
	if (!entry) throw std::invalid_argument("Unregistered component type: " + std::string(key));
	JsonRead::Object(data);
	if (entry->validate) entry->validate(data, SerializationContext{ context.projectRoot, this });
	return entry->factory(owner, data, context);
}

const ComponentRegistry& ComponentRegistry::Builtins()
{
	static const ComponentRegistry registry = [] { ComponentRegistry result; RegisterBuiltinComponents(result); return result; }();
	return registry;
}
