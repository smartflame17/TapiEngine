#pragma once
#include <algorithm>
#include <vector>
#include <memory>
#include <string>
#include <cstdint>
#include <type_traits>
#include <utility>
#include "Scene.h"
#include "../Components/Component.h"

class Scene;
class Component;
class Graphics;

class GameObject
{
public:
	GameObject(Scene& ownerScene, std::string objectName);
	~GameObject() = default;
	GameObject(const GameObject&) = delete;
	GameObject& operator=(const GameObject&) = delete;

	std::uint64_t GetId() const noexcept;
	const std::string& GetName() const noexcept;
	void SetName(std::string newName) noexcept;

	GameObject* GetParent() const noexcept;
	const std::vector<std::unique_ptr<GameObject>>& GetChildren() const noexcept;
	const std::vector<std::unique_ptr<Component>>& GetComponents() const noexcept;


	GameObject& AddChild(std::unique_ptr<GameObject> child) noexcept;
	std::unique_ptr<GameObject> DetachChild(GameObject& child) noexcept;

	template<typename T, typename... Args>
	T& AddComponent(Args&&... args)
	{
		static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");
		auto component = std::make_unique<T>(std::forward<Args>(args)...);
		component->SetOwner(this);
		T& componentRef = *component;
		components.push_back(std::move(component));
		return componentRef;
	}

	template<typename T>
	T* GetComponent() noexcept
	{
		static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");
		for (auto& component : components)
		{
			if (auto casted = dynamic_cast<T*>(component.get()))
				return casted;
		}
		return nullptr;
	}

	template<typename T>
	const T* GetComponent() const noexcept
	{
		static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");
		for (const auto& component : components)
		{
			if (const auto casted = dynamic_cast<const T*>(component.get()))
				return casted;
		}
		return nullptr;
	}

	template<typename T>
	bool RemoveComponent() noexcept
	{
		static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");
		for (auto it = components.begin(); it != components.end(); ++it)
		{
			if (dynamic_cast<T*>(it->get()) != nullptr)
			{
				components.erase(it);
				return true;
			}
		}
		return false;
	}

	void Update(float dt, bool isSimulationRunning) noexcept;
	void Render(Graphics& gfx) const noexcept(!IS_DEBUG);

private:
	void SetParent(GameObject* newParent) noexcept;

private:
	static std::uint64_t nextId;
	std::uint64_t id = 0;
	Scene& scene;
	std::string name;
	GameObject* parent = nullptr;
	std::vector<std::unique_ptr<GameObject>> children;
	std::vector<std::unique_ptr<Component>> components;
};
