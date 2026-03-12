#include "GameObject.h"

std::uint64_t GameObject::nextId = 1;

GameObject::GameObject(Scene& ownerScene, std::string objectName) :
	id(nextId++),
	scene(ownerScene),
	name(std::move(objectName))
{}

std::uint64_t GameObject::GetId() const noexcept
{
	return id;
}

const std::string& GameObject::GetName() const noexcept
{
	return name;
}

void GameObject::SetName(std::string newName) noexcept
{
	name = std::move(newName);
}

GameObject* GameObject::GetParent() const noexcept
{
	return parent;
}

std::vector<std::unique_ptr<GameObject>>& GameObject::GetChildren() noexcept
{
	return children;
}

const std::vector<std::unique_ptr<GameObject>>& GameObject::GetChildren() const noexcept
{
	return children;
}

std::vector<std::unique_ptr<Component>>& GameObject::GetComponents() noexcept
{
	return components;
}

const std::vector<std::unique_ptr<Component>>& GameObject::GetComponents() const noexcept
{
	return components;
}

GameObject& GameObject::AddChild(std::unique_ptr<GameObject> child) noexcept
{
	child->SetParent(this);
	GameObject& childRef = *child;
	children.push_back(std::move(child));
	return childRef;
}

std::unique_ptr<GameObject> GameObject::DetachChild(GameObject& child) noexcept
{
	auto it = std::find_if(children.begin(), children.end(), [&](const auto& current) { return current.get() == &child; });
	if (it == children.end())
		return nullptr;

	(*it)->SetParent(nullptr);
	auto out = std::move(*it);
	children.erase(it);
	return out;
}

void GameObject::Update(float dt, bool isSimulationRunning) noexcept
{
	for (auto& component : components)
	{
		component->OnUpdate(dt, isSimulationRunning);
	}

	for (auto& child : children)
	{
		child->Update(dt, isSimulationRunning);
	}
}

void GameObject::Render(Graphics& gfx) const noexcept(!IS_DEBUG)
{
	for (const auto& component : components)
	{
		component->OnRender(gfx);
	}

	for (const auto& child : children)
	{
		child->Render(gfx);
	}
}

void GameObject::SetParent(GameObject* newParent) noexcept
{
	parent = newParent;
}
