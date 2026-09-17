#pragma once
#include <algorithm>
#include <vector>
#include <memory>
#include <string>
#include <cstdint>
#include <type_traits>
#include <utility>
#include <stdexcept>
#include <DirectXMath.h>
#include "Scene.h"
#include "Transform.h"
#include "../Components/Component.h"
#include "../Components/CustomBehaviour.h"
#include "../Components/DrawableComponent.h"
#include "spdlog/spdlog.h"

class Graphics;
class Animator;
class Rigidbody;
class Collider;
struct b3BodyId;

class GameObject
{
public:
	GameObject(Scene& ownerScene, std::string objectName);
	GameObject(const GameObject&) = delete;
	~GameObject();
	GameObject& operator=(const GameObject&) = delete;

	std::uint64_t GetId() const noexcept;
	const std::string& GetName() const noexcept;
	void SetName(std::string newName) noexcept;
	bool IsStatic() const noexcept;
	void SetStatic(bool value) noexcept;

	GameObject* GetParent() const noexcept;
	const std::vector<std::unique_ptr<GameObject>>& GetChildren() const noexcept;
	const std::vector<std::unique_ptr<Component>>& GetComponents() const noexcept;
	Scene& GetScene() const noexcept;
	bool IsPendingKill() const noexcept;
	void Destroy() noexcept;

	void SetPosition(float x, float y, float z) noexcept;
	void SetRotation(float x, float y, float z) noexcept;
	void SetScale(float x, float y, float z) noexcept;
	void SetTransform(const Transform& newTransform) noexcept;

	const Transform& GetTransform() const noexcept;
	DirectX::XMMATRIX GetLocalTransformMatrix() const noexcept;
	DirectX::XMMATRIX GetWorldTransformMatrix() const noexcept;

	GameObject& AddChild(std::unique_ptr<GameObject> child) noexcept;
	std::unique_ptr<GameObject> DetachChild(GameObject& child) noexcept;

	CustomBehaviour* AddScript(const std::string& scriptName);

	template<typename Fn>
	void ForEachScript(Fn&& fn) noexcept
	{
		for (auto& component : components)
		{
			if (component->isType<CustomBehaviour>())
			{
				auto* script = static_cast<CustomBehaviour*>(component.get());
				fn(*script);
			}
		}

		for (auto& child : children)
		{
			child->ForEachScript(std::forward<Fn>(fn));
		}
	}

	template<typename T, typename... Args>
	T& AddComponent(Args&&... args)
	{
		static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");
		if (isPendingKill) throw std::logic_error("Cannot add components to a destroyed GameObject.");
		if constexpr (std::is_same_v<T, Animator> || std::is_same_v<T, Rigidbody> || std::is_same_v<T, Collider>)
		{
			for (const auto& existing : components)
				if (existing->IsType(T::StaticType) && !existing->IsPendingInspectorRemoval())
					throw std::logic_error("Only one component of this type is allowed per GameObject.");
		}
		auto component = std::make_unique<T>(std::forward<Args>(args)...);
		component->SetOwner(this);
		T& componentRef = *component;
		components.push_back(std::move(component));
		try { InitializePhysicsComponent(componentRef); }
		catch (...) { components.pop_back(); throw; }

		//TODO: can optimize by caching component type in GameObject, so we don't have to check every component for every type query
		// also needs to handle other compoent types like lights, camera etc. that require registration to other systems
		if constexpr (std::is_base_of_v<DrawableComponent, T>)
		{
			scene.RegisterDrawable(&componentRef);
		}
		if constexpr (std::is_base_of_v<CustomBehaviour, T>)
		{
			componentRef.ConfigureLifecycle(BuildScriptLifecycleMask<T>());
			scene.RegisterScript(componentRef);
		}
		return componentRef;
	}

	template<typename T>
	T* GetComponent() noexcept
	{
		static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");
		for (auto& component : components)
		{
			if (component->isType<T>() && !component->IsPendingInspectorRemoval())
			{
				return static_cast<T*>(component.get());
			}
		}
		return nullptr;
	}

	template<typename T>
	const T* GetComponent() const noexcept
	{
		static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");
		for (const auto& component : components)
		{
			if (component->isType<T>() && !component->IsPendingInspectorRemoval())
			{
				return static_cast<const T*>(component.get());
			}
		}
		return nullptr;
	}

	template<typename T>
	bool RemoveComponent() noexcept
	{
		static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");
		for (auto& component : components)
		{
			if (component->isType<T>() && !component->IsPendingInspectorRemoval())
			{
				scene.QueueComponentRemoval(*component);
				return true;
			}
		}
		return false;
	}

	void Update(float dt, bool isSimulationRunning) noexcept;

private:
	void InitializePhysicsComponent(Component& component);
	void DetachPhysicsComponent(Component& component) noexcept;
	void ReleasePhysics() noexcept;
	void RefreshPhysics() noexcept;
	void RecreatePhysicsBody() noexcept;
	void NotifyPhysicsTransformChanged() noexcept;
	bool GetPhysicsWorldPose(DirectX::XMFLOAT3& position, DirectX::XMFLOAT4& rotation, DirectX::XMFLOAT3& scale) const noexcept;
	b3BodyId GetPhysicsBody() const noexcept;
	void CapturePhysicsPose() noexcept;
	void ApplyPhysicsPose() noexcept;
	void MarkPendingKill() noexcept;
	template<typename T>
	static constexpr std::uint8_t BuildScriptLifecycleMask() noexcept
	{
		if constexpr (!std::is_base_of_v<CustomBehaviour, T>)
		{
			return CustomBehaviour::None;
		}
		else
		{
			return ::BuildScriptLifecycleMask<T>();
		}
	}

	void SetParent(GameObject* newParent) noexcept;

private:
	static std::uint64_t nextId;
	std::uint64_t id = 0;
	Scene& scene;
	std::string name;
	bool isStatic = false;
	GameObject* parent = nullptr;
	Transform transform;
	std::vector<std::unique_ptr<GameObject>> children;
	std::vector<std::unique_ptr<Component>> components;
	bool isPendingKill = false;
	DirectX::XMFLOAT3 physicsPosition{};
	DirectX::XMFLOAT4 physicsRotation{ 0, 0, 0, 1 };
	bool hasPhysicsPose = false;

	friend class Scene;
	friend class Rigidbody;
	friend class Collider;
};

template<typename T>
inline T* Component::GetComponent() noexcept
{
	return GetGameObject().GetComponent<T>();
}

template<typename T>
inline const T* Component::GetComponent() const noexcept
{
	return GetGameObject().GetComponent<T>();
}

#ifndef TE_LOG
#define TE_LOG(...) SPDLOG_INFO(__VA_ARGS__)
#endif

#ifndef TE_LOGERROR
#define TE_LOGERROR(...) SPDLOG_ERROR(__VA_ARGS__)
#endif

#ifndef TE_LOGWARNING
#define TE_LOGWARNING(...) SPDLOG_WARN(__VA_ARGS__)
#endif
