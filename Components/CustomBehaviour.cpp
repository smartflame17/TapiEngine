#include "CustomBehaviour.h"
#include "../Scene/GameObject.h"
#include "../Scene/Scene.h"
#include "../imgui/imgui.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <typeinfo>

void CustomBehaviour::Awake()
{
}

void CustomBehaviour::OnEnable()
{
}

void CustomBehaviour::Start()
{
}

void CustomBehaviour::Update(float dt)
{
}

void CustomBehaviour::FixedUpdate()
{
}

void CustomBehaviour::LateUpdate(float dt)
{
}

void CustomBehaviour::OnDisable()
{
}

void CustomBehaviour::OnDestroy()
{
}

bool CustomBehaviour::IsEnabled() const noexcept
{
	return isEnabled;
}

void CustomBehaviour::SetEnabled(bool enabled) noexcept
{
	if (isEnabled == enabled)
	{
		return;
	}

	isEnabled = enabled;

	if (auto* owner = TryGetGameObject())
	{
		owner->GetScene().HandleScriptEnableStateChanged(*this);
	}
}

bool CustomBehaviour::HasStarted() const noexcept
{
	return hasStarted;
}

Scene& CustomBehaviour::GetScene() const noexcept
{
	return GetGameObject().GetScene();
}

bool CustomBehaviour::SupportsAwake() const noexcept
{
	return (lifecycleMask & AwakeFlag) != 0;
}

bool CustomBehaviour::SupportsOnEnable() const noexcept
{
	return (lifecycleMask & OnEnableFlag) != 0;
}

bool CustomBehaviour::SupportsStart() const noexcept
{
	return (lifecycleMask & StartFlag) != 0;
}

bool CustomBehaviour::SupportsUpdate() const noexcept
{
	return (lifecycleMask & UpdateFlag) != 0;
}

bool CustomBehaviour::SupportsFixedUpdate() const noexcept
{
	return (lifecycleMask & FixedUpdateFlag) != 0;
}

bool CustomBehaviour::SupportsLateUpdate() const noexcept
{
	return (lifecycleMask & LateUpdateFlag) != 0;
}

bool CustomBehaviour::SupportsOnDisable() const noexcept
{
	return (lifecycleMask & OnDisableFlag) != 0;
}

bool CustomBehaviour::SupportsOnDestroy() const noexcept
{
	return (lifecycleMask & OnDestroyFlag) != 0;
}

void CustomBehaviour::ConfigureLifecycle(std::uint8_t mask) noexcept
{
	lifecycleMask = mask;
}

void CustomBehaviour::MarkStarted() noexcept
{
	hasStarted = true;
}

void CustomBehaviour::MarkEnableNotified(bool notified) noexcept
{
	enableNotified = notified;
}

bool CustomBehaviour::WasEnableNotified() const noexcept
{
	return enableNotified;
}

void CustomBehaviour::MarkQueuedForDestroy(bool queued) noexcept
{
	queuedForDestroy = queued;
}

bool CustomBehaviour::IsQueuedForDestroy() const noexcept
{
	return queuedForDestroy;
}

void CustomBehaviour::OnInspector() noexcept
{
	const char* scriptName = typeid(*this).name();
	if (const char* classPrefix = std::strstr(scriptName, "class "); classPrefix != nullptr)
	{
		scriptName = classPrefix + 6;
	}

	if (!ImGui::TreeNodeEx(scriptName, ImGuiTreeNodeFlags_DefaultOpen))
	{
		return;
	}

	bool enabled = IsEnabled();
	if (ImGui::Checkbox("Enabled", &enabled))
	{
		SetEnabled(enabled);
	}

	properties.clear();
	ExposeVariables();

	for (std::size_t index = 0; index < properties.size(); ++index)
	{
		ExposedProperty& property = properties[index];
		ImGui::PushID(static_cast<int>(index));

		switch (property.type)
		{
		case PropertyType::Int:
			ImGui::InputInt(property.name.c_str(), static_cast<int*>(property.value));
			break;

		case PropertyType::Float:
			ImGui::InputFloat(property.name.c_str(), static_cast<float*>(property.value));
			break;

		case PropertyType::String:
		{
			auto* value = static_cast<std::string*>(property.value);
			std::vector<char> buffer(std::max<std::size_t>(value->size() + 1, 256), '\0');
			std::memcpy(buffer.data(), value->c_str(), value->size());
			if (ImGui::InputText(property.name.c_str(), buffer.data(), buffer.size()))
			{
				*value = buffer.data();
			}
			break;
		}

		case PropertyType::Vector3:
			ImGui::InputFloat3(property.name.c_str(), static_cast<float*>(property.value));
			break;

		case PropertyType::Color:
			ImGui::ColorEdit3(property.name.c_str(), static_cast<float*>(property.value));
			break;

		case PropertyType::Bool:
			ImGui::Checkbox(property.name.c_str(), static_cast<bool*>(property.value));
			break;
		}

		ImGui::PopID();
	}

	ImGui::TreePop();
}
