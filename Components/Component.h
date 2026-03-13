#pragma once

#include <cstdint>
#include <cassert>

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

	virtual void OnUpdate(float dt, bool isSimulationRunning) noexcept;
	virtual void OnInspector() noexcept;

private:
	static std::uint64_t nextId;
	std::uint64_t id = 0;
	GameObject* owner = nullptr;
};
