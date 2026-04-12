#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <type_traits>
#include <unordered_map>

class CustomBehaviour;
class GameObject;

class ScriptRegistry
{
public:
	using ScriptFactory = std::function<CustomBehaviour*(GameObject*)>;

	struct ScriptEntry
	{
		ScriptFactory factory;
		std::uint8_t lifecycleMask = 0;
	};

	static ScriptRegistry& GetInstance() noexcept
	{
		static ScriptRegistry instance;
		return instance;
	}

	template<typename T>
	void Register(const std::string& name, std::uint8_t lifecycleMask)
	{
		static_assert(std::is_base_of_v<CustomBehaviour, T>, "Registered script must derive from CustomBehaviour");
		registry[name] = ScriptEntry{
			[](GameObject* owner) -> CustomBehaviour*
			{
				return new T(owner);
			},
			lifecycleMask
		};
	}

	CustomBehaviour* Create(const std::string& name, GameObject* owner) const noexcept;
	bool IsRegistered(const std::string& name) const noexcept;

private:
	std::unordered_map<std::string, ScriptEntry> registry;
};
