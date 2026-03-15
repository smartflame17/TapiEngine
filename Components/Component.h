#pragma once

#include <cstdint>
#include <cassert>
#include <DirectXMath.h>
#include "../Scene/Transform.h"

class GameObject;
class Graphics;

class Component
{
public:
	Component();
	virtual ~Component() = default;

	void SetOwner(GameObject* gameObject) noexcept;
	GameObject& GetGameObject() const noexcept;
	std::uint64_t GetId() const noexcept;

	Transform& GetTransform() noexcept;
	const Transform& GetTransform() const noexcept;
	DirectX::XMMATRIX GetLocalTransformMatrix() const noexcept;
	DirectX::XMMATRIX GetWorldTransformMatrix() const noexcept;

	virtual void OnUpdate(float dt, bool isSimulationRunning) noexcept;
	virtual void OnInspector() noexcept;

	template<typename T>
	T* GetComponent() noexcept;

	template<typename T>
	const T* GetComponent() const noexcept;
	
private:
	static std::uint64_t nextId;
	std::uint64_t id = 0;
	GameObject* owner = nullptr;
	Transform transform;
};
