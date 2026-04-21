#pragma once

#include <cstdint>
#include <cassert>
#include <string_view>

class GameObject;
class Graphics;

enum class ComponentType : std::uint8_t
{
	Drawable,
	CustomBehaviour,
	SpotLight, 
	PointLight,
	DirectionalLight,
	Camera,
	Other,
	Count
};

constexpr std::string_view ComponentTypeToString(ComponentType type) noexcept
{
	switch (type)
	{
	case ComponentType::Drawable:
		return "Drawable";
	case ComponentType::CustomBehaviour:
		return "Custom Behaviour";
	case ComponentType::SpotLight:
		return "Spot Light";
	case ComponentType::PointLight:
		return "Point Light";
	case ComponentType::DirectionalLight:
		return "Directional Light";
	case ComponentType::Camera:
		return "Camera";
	case ComponentType::Other:
		return "Other";
	default:
		assert(false && "Invalid ComponentType");
		return "Unknown";
	}
}


class Component
{
public:
	explicit Component(ComponentType type = ComponentType::Other) noexcept;
	virtual ~Component() = default;

	void SetOwner(GameObject* gameObject) noexcept;
	GameObject& GetGameObject() const noexcept;
	GameObject* TryGetGameObject() const noexcept;
	std::uint64_t GetId() const noexcept;

	ComponentType GetType() const noexcept { return type; }
	bool IsType(ComponentType componentType) const noexcept { return type == componentType; }

	template<typename T>
	bool isType() const noexcept
	{
		return type == T::StaticType;
	}

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

public:
	
private:
	static std::uint64_t nextId;
	std::uint64_t id = 0;
	GameObject* owner = nullptr;
	bool inspectorCollapsed = false;
	bool pendingInspectorRemoval = false;
	ComponentType type = ComponentType::Other;
};
