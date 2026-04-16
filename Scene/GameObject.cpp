#include "GameObject.h"
#include "../Components/CustomBehaviour.h"

std::uint64_t GameObject::nextId = 1;

GameObject::GameObject(Scene& ownerScene, std::string objectName) :
	id(nextId++),
	scene(ownerScene),
	name(std::move(objectName))
{}

GameObject::~GameObject()
{
	for (auto& component : components)
	{
		if (auto drawable = dynamic_cast<DrawableComponent*>(component.get()))
		{
			scene.UnregisterDrawable(drawable);
		}
	}
}

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

bool GameObject::IsStatic() const noexcept
{
	return isStatic;
}

void GameObject::SetStatic(bool value) noexcept
{
	isStatic = value;
}

GameObject* GameObject::GetParent() const noexcept
{
	return parent;
}

const std::vector<std::unique_ptr<GameObject>>& GameObject::GetChildren() const noexcept
{
	return children;
}

const std::vector<std::unique_ptr<Component>>& GameObject::GetComponents() const noexcept
{
	return components;
}

Scene& GameObject::GetScene() const noexcept
{
	return scene;
}

bool GameObject::IsPendingKill() const noexcept
{
	return isPendingKill;
}

void GameObject::Destroy() noexcept
{
	scene.DestroyGameObject(*this);
}

void GameObject::SetPosition(float x, float y, float z) noexcept
{
	transform.position = { x, y, z };
}

void GameObject::SetRotation(float x, float y, float z) noexcept
{
	transform.rotation = { x, y, z };
}
void GameObject::SetScale(float x, float y, float z) noexcept
{
	transform.scale = { x, y, z };
}

void GameObject::SetTransform(const Transform& newTransform) noexcept
{
	transform = newTransform;
}

Transform& GameObject::GetTransform() noexcept
{
	return transform;
}

const Transform& GameObject::GetTransform() const noexcept
{
	return transform;
}

DirectX::XMMATRIX GameObject::GetLocalTransformMatrix() const noexcept
{
	return MakeTransformMatrix(transform);
}

DirectX::XMMATRIX GameObject::GetWorldTransformMatrix() const noexcept
{
	if (parent == nullptr)
	{
		return GetLocalTransformMatrix();
	}
	return GetLocalTransformMatrix() * parent->GetWorldTransformMatrix();
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
		if (dynamic_cast<CustomBehaviour*>(component.get()) != nullptr)
		{
			continue;
		}
		component->OnUpdate(dt, isSimulationRunning);
	}

	for (auto& child : children)
	{
		if (child->IsPendingKill())
		{
			continue;
		}
		child->Update(dt, isSimulationRunning);
	}
}

void GameObject::MarkPendingKill() noexcept
{
	isPendingKill = true;
}

void GameObject::SetParent(GameObject* newParent) noexcept
{
	parent = newParent;
}

CustomBehaviour* GameObject::AddScript(const std::string& scriptName)
{
	CustomBehaviour* script = ScriptRegistry::GetInstance().Create(scriptName, this);
	if (script)
	{
		script->SetScriptName(scriptName);

		std::unique_ptr<Component> componentPtr(script);
		components.push_back(std::move(componentPtr));
		scene.RegisterScript(*script);		// Lifecycle mask is already configured in the registry, so no need to configure again here

		return script;
	}
	TE_LOGERROR("Failed to add script '%s' to GameObject '%s': Script not found in registry", scriptName.c_str(), name.c_str());
	return nullptr;
}