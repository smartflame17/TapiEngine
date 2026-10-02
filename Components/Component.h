#pragma once

#include <cstdint>
#include <cassert>
#include <string_view>
#include "../Serialization/Guid.h"

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
	Animator,
	Rigidbody,
	Collider,
	AudioClip,
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
	case ComponentType::Animator:
		return "Animator";
	case ComponentType::Rigidbody:
		return "Rigidbody";
	case ComponentType::Collider:
		return "Collider";
	case ComponentType::AudioClip:
		return "Audio Clip";
	default:
		assert(false && "Invalid ComponentType");
		return "Unknown";
	}
}


class Component
{
public:
	explicit Component(ComponentType type = ComponentType::Other);
	Component(const Component&) = delete;
	Component& operator=(const Component&) = delete;
	virtual ~Component() = default;

	void SetOwner(GameObject* gameObject) noexcept;
	GameObject& GetGameObject() const noexcept;
	GameObject* TryGetGameObject() const noexcept;
	const Guid& GetId() const noexcept;
	// Used to restore saved identities; scene-wide uniqueness is checked by the loader.
	void SetId(const Guid& savedId);

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
	Guid id = Guid::Generate();
	GameObject* owner = nullptr;
	bool pendingInspectorRemoval = false;
	ComponentType type = ComponentType::Other;
};
