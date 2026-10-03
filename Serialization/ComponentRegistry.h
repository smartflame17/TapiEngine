#pragma once
#include "Json.h"
#include "SerializationContext.h"
#include "../Components/Component.h"
#include <functional>
#include <vector>

class GameObject;

struct ComponentRegistration
{
	using Factory = std::function<Component&(GameObject&, const nlohmann::json&, LoadContext&)>;
	using Validator = std::function<void(const nlohmann::json&, const SerializationContext&)>;
	std::string key;
	ComponentType type = ComponentType::Other;
	int loadOrder = 0;
	bool singlePerObject = false;
	bool sceneSupported = true;
	Factory factory;
	Validator validate;
};

class ComponentRegistry
{
public:
	void Register(ComponentRegistration registration);
	const ComponentRegistration* Find(std::string_view key) const noexcept;
	const ComponentRegistration* Find(ComponentType type) const noexcept;
	Component& Create(std::string_view key, GameObject& owner, const nlohmann::json& data, LoadContext& context) const;
	bool DrawEditorControls(GameObject& owner, ComponentType type, LoadContext& context) const;
	const std::vector<ComponentRegistration>& GetEntries() const noexcept { return entries; }
	static const ComponentRegistry& Builtins();
private:
	std::vector<ComponentRegistration> entries;
};
