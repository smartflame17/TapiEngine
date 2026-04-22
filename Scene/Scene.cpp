#include "Scene.h"
#include "GameObject.h"
#include "../Components/Component.h"
#include "../Components/CustomBehaviour.h"
#include "../Components/DrawableComponent.h"
#include "../Graphics/Camera.h"
#include "../Graphics/Drawable/Drawable.h"
#include "../Graphics/Graphics.h"
#include "../Graphics/RenderQueue.h"
#include "../Graphics/Lighting/DirectionalLight.h"
#include "../Graphics/Lighting/PointLight.h"
#include "../Graphics/Lighting/SpotLight.h"
#include <algorithm>
#include <iostream>
#include <unordered_set>

Scene::Scene() : name("Scene") {}

Scene::Scene(const std::string& sceneName) : name(sceneName) {}

Scene::~Scene() { Clear(); }	// fix: explicitly clear the scene to ensure proper destruction order of GameObjects and their components

GameObject& Scene::CreateGameObject(const std::string& name)
{
	auto object = std::make_unique<GameObject>(*this, name);
	GameObject& objectRef = *object;
	rootObjects.push_back(std::move(object));
	return objectRef;
}

GameObject& Scene::CreateChildGameObject(GameObject& parent, const std::string& name)
{
	auto child = std::make_unique<GameObject>(*this, name);
	return parent.AddChild(std::move(child));
}

void Scene::Clear() noexcept
{
	scriptManager.Clear();
	drawables.clear();
	rootObjects.clear();
	pendingComponentRemovals.clear();
	bvhManager.Clear();
	skybox.reset();
	selectedObject = nullptr;
}

void Scene::ProcessScriptAwakeAndStart(bool isSimulationRunning) noexcept
{
	if (!isSimulationRunning)
	{
		return;
	}

	scriptManager.ProcessAwakeAndStart();
}

void Scene::FixedUpdate(bool isSimulationRunning) noexcept
{
	if (!isSimulationRunning)
	{
		return;
	}

	scriptManager.FixedUpdate();
}

void Scene::Update(float dt, bool isSimulationRunning) noexcept
{
	if (isSimulationRunning)
	{
		scriptManager.Update(dt);
	}

	for (auto& object : rootObjects)
	{
		if (object->IsPendingKill())
		{
			continue;
		}
		object->Update(dt, isSimulationRunning);
	}
}

void Scene::LateUpdate(float dt, bool isSimulationRunning) noexcept
{
	if (!isSimulationRunning)
	{
		return;
	}

	scriptManager.LateUpdate(dt);
}

void Scene::CleanupDestroyedObjects() noexcept
{
	scriptManager.Cleanup();

	auto sweep = [&](auto& self, std::vector<std::unique_ptr<GameObject>>& objects) -> void
	{
		for (auto it = objects.begin(); it != objects.end(); )
		{
			GameObject& object = *(*it);
			self(self, object.children);

			if (object.IsPendingKill())
			{
				if (selectedObject == &object)
				{
					selectedObject = nullptr;
				}
				it = objects.erase(it);
			}
			else
			{
				++it;
			}
		}
	};

	sweep(sweep, rootObjects);
}

// Actual cleanup is done after all rendering is done and before next frame
void Scene::CleanupPendingComponentRemovals() noexcept
{
	if (pendingComponentRemovals.empty())
	{
		return;
	}
	std::unordered_set<Component*> pendingSet;
	pendingSet.reserve(pendingComponentRemovals.size());

	for (Component* component : pendingComponentRemovals) // move to set for quick lookup
	{
		if (component != nullptr)
		{
			pendingSet.insert(component);
		}
	}

	// Lambda to unregister components based on their type (may be better to refactor to have components save their type via enum class?)
	auto unregisterComponent = [this](Component& component) noexcept
		{
			if (component.IsType(ComponentType::Drawable))
				UnregisterDrawable(static_cast<DrawableComponent*>(&component));
			/*if (auto* drawable = dynamic_cast<DrawableComponent*>(&component))
			{
				UnregisterDrawable(drawable);
			}*/
			else if (component.IsType(ComponentType::CustomBehaviour))
			{
				auto* script = static_cast<CustomBehaviour*>(&component);
				scriptManager.UnregisterScript(*script);
			}
				
			/*
			if (auto* script = dynamic_cast<CustomBehaviour*>(&component))
			{
				scriptManager.UnregisterScript(*script);
			}*/
		};

	// Run through game objects to remove the component that are pending removal -> we can change this to only run through objects that are related to the pending components if we want to optimize
	auto sweep = [&](auto& self, std::vector<std::unique_ptr<GameObject>>& objects) -> void
		{
			for (auto& object : objects)
			{
				auto& components = object->components;
				for (auto it = components.begin(); it != components.end(); )
				{
					Component* component = it->get();
					if (component != nullptr && pendingSet.find(component) != pendingSet.end())
					{
						unregisterComponent(*component);
						it = components.erase(it);	// free memory (RAII unique ptr)
					}
					else
					{
						++it;
					}
				}

				self(self, object->children);
			}
		};

	sweep(sweep, rootObjects);
	pendingComponentRemovals.clear();
}

void Scene::Submit(RenderQueueBuilder& queueBuilder, const RenderView& view) noexcept(!IS_DEBUG)
{
	if (skybox)
	{
		// lambda submission for skybox rendering, ensures it gets drawn in the correct pass and can set its own states without affecting other drawables
		queueBuilder.SubmitCallback(RenderPassId::Skybox, [this](Graphics& gfx)
			{
				if (skybox)
				{
					skybox->Draw(gfx);
					gfx.RestoreDefaultStates();
				}
			});
	}

	bvhManager.Sync();

	// opaque drawables
	std::vector<DrawableComponent*> visibleDrawables;
	if (view.camera != nullptr)
	{
		const_cast<Camera*>(view.camera)->UpdateFrustum(view.projection);
		bvhManager.QueryVisibleDrawables(view.camera->GetFrustum(), visibleDrawables);
	}
	else
	{
		visibleDrawables = drawables;
	}

	for (const auto* drawable : visibleDrawables)
	{
		if (drawable != nullptr && !drawable->GetGameObject().IsPendingKill())
		{
			drawable->Submit(queueBuilder, view);
		}
	}

	// light gizmos
	auto submitGizmos = [&](auto& self, const GameObject& gameObject) -> void
	{
		if (gameObject.IsPendingKill())
		{
			return;
		}

		for (const auto& component : gameObject.GetComponents())
		{
			if (const auto* pointLight = dynamic_cast<const PointLight*>(component.get()))
			{
				pointLight->SubmitGizmo(queueBuilder);
			}
			else if (const auto* spotLight = dynamic_cast<const SpotLight*>(component.get()))
			{
				spotLight->SubmitGizmo(queueBuilder);
			}
			/*else if (const auto* directionalLight = dynamic_cast<const DirectionalLight*>(component.get()))
			{
				directionalLight->SubmitGizmo(queueBuilder);
			}*/
		}

		for (const auto& child : gameObject.GetChildren())
		{
			self(self, *child);
		}
	};

	for (const auto& rootObject : rootObjects)
	{
		submitGizmos(submitGizmos, *rootObject);
	}
}

void Scene::CollectRenderLights(std::vector<RenderLight>& lights) const noexcept
{
	lights.clear();

	auto collect = [&](auto& self, const GameObject& gameObject) -> void
	{
		if (gameObject.IsPendingKill())
		{
			return;
		}

		for (const auto& component : gameObject.GetComponents())
		{
			if (const auto* pointLight = dynamic_cast<const PointLight*>(component.get()))
			{
				lights.push_back(pointLight->BuildRenderLight());
			}
			else if (const auto* spotLight = dynamic_cast<const SpotLight*>(component.get()))
			{
				lights.push_back(spotLight->BuildRenderLight());
			}
			else if (const auto* directionalLight = dynamic_cast<const DirectionalLight*>(component.get()))
			{
				lights.push_back(directionalLight->BuildRenderLight());
			}
		}

		for (const auto& child : gameObject.GetChildren())
		{
			self(self, *child);
		}
	};

	for (const auto& rootObject : rootObjects)
	{
		collect(collect, *rootObject);
	}
}

void Scene::CollectBVHBounds(std::vector<DirectX::BoundingBox>& bounds) noexcept
{
	bvhManager.Sync();
	bounds.clear();
	bvhManager.CollectHierarchyBounds(bounds);
}

void Scene::SetSkybox(std::unique_ptr<Drawable> drawable)
{
	skybox = std::move(drawable);
}

void Scene::RegisterDrawable(DrawableComponent* drawable) noexcept
{
	drawables.push_back(drawable);
	bvhManager.RegisterDrawable(drawable);
}

void Scene::UnregisterDrawable(DrawableComponent* drawable) noexcept
{
	auto it = std::find(drawables.begin(), drawables.end(), drawable);
	if (it != drawables.end())
	{
		drawables.erase(it);
	}
	bvhManager.UnregisterDrawable(drawable);
}

void Scene::RegisterScript(CustomBehaviour& script) noexcept
{
	scriptManager.RegisterScript(script);
}

void Scene::HandleScriptEnableStateChanged(CustomBehaviour& script) noexcept
{
	scriptManager.HandleEnableStateChanged(script);
}

void Scene::QueueComponentRemoval(Component& component) noexcept // TODO: bug here, the deletion of component is not handled properly
{
	component.MarkPendingInspectorRemoval(true);
	// maybe we dont need duplicate check here?
	if (std::find(pendingComponentRemovals.begin(), pendingComponentRemovals.end(), &component) == pendingComponentRemovals.end())
	{
		pendingComponentRemovals.push_back(&component);
	}
}

void Scene::DestroyGameObject(GameObject& object) noexcept
{
	if (object.IsPendingKill())
	{
		return;
	}

	object.MarkPendingKill();
	for (const auto& child : object.GetChildren())
	{
		DestroyGameObject(*child);
	}

	if (selectedObject == &object)
	{
		selectedObject = nullptr;
	}

	scriptManager.QueueDestroy(object);
}

void Scene::DrawHierarchyWindow() noexcept
{

	ImGui::SetNextWindowSize(ImVec2(300, 720), ImGuiCond_Always);
	ImGui::SetNextWindowPos(ImVec2(0, 60), ImGuiCond_Always);
	if (!ImGui::Begin(name.c_str(), nullptr,
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoCollapse
		))
	{
		ImGui::End();
		return;
	}

	for (auto& object : rootObjects)
	{
		if (object->IsPendingKill())
		{
			continue;
		}
		DrawHierarchyNode(*object);
	}

	ImGui::Separator();
	if (selectedObject != nullptr)
	{
		ImGui::Text("Selected: %s", selectedObject->GetName().c_str());
		//ImGui::Text("GameObject ID: %llu", static_cast<unsigned long long>(selectedObject->GetId()));
	}
	else
	{
		ImGui::TextUnformatted("Selected: <none>");
	}

	ImGui::End();
}

void Scene::SelectGameObjectByRay(const DirectX::SimpleMath::Ray& ray) noexcept
{
	bvhManager.Sync();

	if (auto* drawable = bvhManager.RaycastClosestDrawable(ray))
	{
		GameObject& owner = drawable->GetGameObject();
		selectedObject = owner.IsPendingKill() ? nullptr : &owner;
	}
	else
	{
		selectedObject = nullptr;
	}
}

GameObject* Scene::GetSelectedObject() const noexcept
{
	return selectedObject;
}

void Scene::DrawInspectorWindow() noexcept
{
	ImGui::SetNextWindowSize(ImVec2(340, 800), ImGuiCond_Always);
	ImGui::SetNextWindowPos(ImVec2(1580, 15), ImGuiCond_Always);
	if (!ImGui::Begin("Inspector", nullptr,
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_AlwaysVerticalScrollbar
		))
	{
		ImGui::End();
		return;
	}

	if (selectedObject == nullptr || selectedObject->IsPendingKill())
	{
		selectedObject = nullptr;
		ImGui::TextUnformatted("No GameObject selected.");
		ImGui::End();
		return;
	}

	ImGui::Text("GameObject: %s", selectedObject->GetName().c_str());
	ImGui::Text("ID: %llu", static_cast<unsigned long long>(selectedObject->GetId()));
	ImGui::Separator();
	bool isStatic = selectedObject->IsStatic();
	if (ImGui::Checkbox("Static", &isStatic))
	{
		selectedObject->SetStatic(isStatic);
	}
	ImGui::Separator();

	auto& objectTransform = selectedObject->GetTransform();
	ImGui::Text("Transform");
	ImGui::DragFloat3("Position", &objectTransform.position.x, 0.05f);
	DirectX::XMFLOAT3 rotationDegrees = {
		DirectX::XMConvertToDegrees(objectTransform.rotation.x),
		DirectX::XMConvertToDegrees(objectTransform.rotation.y),
		DirectX::XMConvertToDegrees(objectTransform.rotation.z)
	};
	if (ImGui::DragFloat3("Rotation", &rotationDegrees.x, 0.5f))
	{
		objectTransform.rotation.x = DirectX::XMConvertToRadians(rotationDegrees.x);
		objectTransform.rotation.y = DirectX::XMConvertToRadians(rotationDegrees.y);
		objectTransform.rotation.z = DirectX::XMConvertToRadians(rotationDegrees.z);
	}
	ImGui::DragFloat3("Scale", &objectTransform.scale.x, 0.05f, 0.01f, 200.0f, "%.2f");
	ImGui::Separator();

	const auto& components = selectedObject->GetComponents();
	if (components.empty())
	{
		ImGui::TextUnformatted("No components.");
	}
	else
	{
		for (const auto& component : components)
		{
			if (component != nullptr && !component->IsPendingInspectorRemoval())
			{
				component->OnInspector();
				ImGui::Spacing();
			}
		}
	}
	ImGui::Separator();
	if (ImGui::Button("Add Component"))
	{
		ImGui::OpenPopup("AddComponentPopup");
	}

	DrawAddComponentPopup();

	ImGui::End();
}

const std::vector<Component*>& Scene::GetPendingComponentRemovals() const noexcept
{
	return pendingComponentRemovals;
}

inline void Scene::DrawAddComponentPopup() noexcept
{
	ImGui::SetNextWindowSize(ImVec2(600, 380), ImGuiCond_Always);
	if (ImGui::BeginPopupModal("AddComponentPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		//Left child panel for component type selection
		static int selectedComponentType = 0;
		{
			ImGui::BeginChild("ComponentTypeSelection", ImVec2(150, 320), ImGuiChildFlags_Borders);
			for (int i = 0; i < static_cast<int>(ComponentType::Count); ++i)
			{
				const bool isSelected = (selectedComponentType == i);
				if (ImGui::Selectable(ComponentTypeToString(static_cast<ComponentType>(i)).data(), isSelected,	// as component type string is created from string literal, it is null terminated and safe to use data() here
					ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_SelectOnNav))
				{
					selectedComponentType = i;
				}
			}
			ImGui::EndChild();
		}
		ImGui::SameLine();
		ImGui::BeginGroup();
		// Right child panel for component type description and add button
		{
			ImGui::BeginChild("ComponentTypeDescription", ImVec2(0, 320), ImGuiChildFlags_Borders);
			ImGui::Text("Selected component type = %s", ComponentTypeToString(static_cast<ComponentType>(selectedComponentType)).data());

			// TODO: Add descriptions and functionality for adding components
			bool addComponentResult = false;
				const auto type = static_cast<ComponentType>(selectedComponentType);
				if (selectedObject != nullptr && addComponentHandler != nullptr)
				{
					addComponentResult = addComponentHandler(*selectedObject, type);
				}
				if (selectedObject == nullptr)
				{
					TE_LOGERROR("No GameObject selected to add component of type '%s'", ComponentTypeToString(type).data());
				}
				else if (addComponentHandler == nullptr)
				{
					TE_LOGERROR("No AddComponentHandler set in Scene to handle adding component of type '%s'", ComponentTypeToString(type).data());
				}
				/*else if (!addComponentResult)
				{
					TE_LOGERROR("AddComponentHandler failed to add component of type '%s' to GameObject '%s'", ComponentTypeToString(type).data(), selectedObject->GetName().c_str());
				}*/
			
			/*const auto scriptNames = ScriptRegistry::GetInstance().GetRegisteredScriptNames();
			switch (static_cast<ComponentType>(selectedComponentType))
			{
			case ComponentType::Drawable:
				break;
			case ComponentType::CustomBehaviour:
				for (const auto& scriptName : scriptNames)
				{
					if (ImGui::Button(("Add " + scriptName).c_str()))
					{
						if (ScriptRegistry::GetInstance().IsRegistered(scriptName))
						{
							selectedObject->AddScript(scriptName);
							TE_LOG("Added script '%s' to GameObject '%s'", scriptName.c_str(), selectedObject->GetName().c_str());
							ImGui::CloseCurrentPopup();						
						}
						else TE_LOGERROR("Script '%s' is not registered in the ScriptRegistry", scriptName.c_str());
					}
				}

				break;
			case ComponentType::SpotLight:
				if (ImGui::Button("Add Spot Light"))
				{
					selectedObject->AddComponent<SpotLight>();
					ImGui::CloseCurrentPopup();
				}
				break;
			case ComponentType::PointLight:
				if (ImGui::Button("Add Point Light"))
				{
					selectedObject->AddComponent<PointLight>();
					ImGui::CloseCurrentPopup();
				}
				break;
			case ComponentType::DirectionalLight:
				if (ImGui::Button("Add Directional Light"))
				{
					selectedObject->AddComponent<DirectionalLight>();
					ImGui::CloseCurrentPopup();
				}
				break;
			case ComponentType::Camera:
				if (ImGui::Button("Add Camera"))
				{
					selectedObject->AddComponent<Camera>();
					ImGui::CloseCurrentPopup();
				}
				break;
			case ComponentType::Other:
				break;
			default:
				TE_LOGERROR("Unknown component type selected in AddComponentPopup");
			}*/
			ImGui::EndChild();
			ImGui::EndGroup();
		}
		if (ImGui::Button("Close"))
		{
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

const std::vector<std::unique_ptr<GameObject>>& Scene::GetRootObjects() const noexcept
{
	return rootObjects;
}

DirectX::XMMATRIX Scene::GetSelectedWorldTransformMatrix() const noexcept
{
	if (selectedObject != nullptr)
	{
		return selectedObject->GetWorldTransformMatrix();
	}
	return DirectX::XMMatrixIdentity();
}

void Scene::SetSelectedWorldTransformMatrix(DirectX::FXMMATRIX matrix) noexcept
{
	if (selectedObject != nullptr)
	{
		const auto* parent = selectedObject->GetParent();
		if (parent != nullptr)
		{
			const auto parentWorld = parent->GetWorldTransformMatrix();
			const auto local = matrix * DirectX::XMMatrixInverse(nullptr, parentWorld);
			selectedObject->GetTransform() = MakeTransformFromMatrix(local);
		}
		else
		{
			selectedObject->GetTransform() = MakeTransformFromMatrix(matrix);
		}
	}
}

// Recursively draws GameObject and its children in the hierarchy window
void Scene::DrawHierarchyNode(GameObject& object) noexcept
{
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_DrawLinesToNodes;
	if (object.GetChildren().empty() && object.GetComponents().empty())
		flags |= ImGuiTreeNodeFlags_Leaf;
	if (selectedObject == &object)
		flags |= ImGuiTreeNodeFlags_Selected;

	const bool opened = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<uintptr_t>(object.GetId())), flags, "%s", object.GetName().c_str());
	if (ImGui::IsItemClicked())
	{
		selectedObject = &object;
	}

	if (opened)
	{
		for (auto& component : object.GetComponents())
		{
			if (component == nullptr)
			{
				continue;
			}
			ImGui::TreeNodeEx(
				reinterpret_cast<void*>(static_cast<uintptr_t>(component->GetId())),
				ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen,
				"Component (%llu)",
				static_cast<unsigned long long>(component->GetId())
			);
			if (ImGui::IsItemClicked())
			{
				selectedObject = &object;
			}
		}

		for (auto& child : object.GetChildren())
		{
			if (child->IsPendingKill())
			{
				continue;
			}
			DrawHierarchyNode(*child);
		}
		ImGui::TreePop();
	}
}

void Scene::SetAddComponentHandler(AddComponentHandler handler) noexcept
{
	addComponentHandler = std::move(handler);
}
