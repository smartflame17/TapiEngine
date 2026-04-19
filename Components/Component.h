#pragma once

#include <cstdint>
#include <cassert>
#include <string>

class GameObject;
class Graphics;

class Component
{
public:
	Component();
	virtual ~Component() = default;

	void SetOwner(GameObject* gameObject) noexcept;
	GameObject& GetGameObject() const noexcept;
	GameObject* TryGetGameObject() const noexcept;
	std::uint64_t GetId() const noexcept;

	virtual void OnUpdate(float dt, bool isSimulationRunning) noexcept;
	virtual void OnInspector() noexcept;
	bool IsPendingInspectorRemoval() const noexcept;
	void MarkPendingInspectorRemoval(bool pending) noexcept;

	template<typename T>
	T* GetComponent() noexcept;

	template<typename T>
	const T* GetComponent() const noexcept;
	
private:
	virtual const char* GetInspectorTitle() const noexcept;
	virtual void DrawInspectorContents() noexcept;

private:
	static std::uint64_t nextId;
	std::uint64_t id = 0;
	GameObject* owner = nullptr;
	bool inspectorCollapsed = false;
	bool pendingInspectorRemoval = false;
};

enum class ComponentType : std::uint8_t
{
	Drawable,
	CustomBehaviour,
	Other,
	Count
};

static inline std::string ComponentTypeToString(ComponentType type)
{
	switch (type)
	{
	case ComponentType::Drawable:
		return "Drawable";
	case ComponentType::CustomBehaviour:
		return "Custom Behaviour";
	case ComponentType::Other:
		return "Other";
	default:
		assert(false && "Invalid ComponentType");
		return "Unknown";
	}
}
