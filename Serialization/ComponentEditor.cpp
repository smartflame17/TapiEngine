#include "ComponentRegistry.h"
#include "JsonEnums.h"
#include "../Scene/GameObject.h"
#include "../Scene/ScriptRegistry.h"
#include "../imgui/imgui.h"

bool ComponentRegistry::DrawEditorControls(GameObject& owner, ComponentType type, LoadContext& context) const
{
	const auto* entry = Find(type);
	if (!entry) { ImGui::TextUnformatted("No component factory is registered."); return false; }
	static std::string error;
	auto create = [&](const nlohmann::json& metadata) {
		try { Create(entry->key, owner, metadata, context); error.clear(); ImGui::CloseCurrentPopup(); return true; }
		catch (const std::exception& failure) { error = failure.what(); return false; }
	};
	bool created = false;
	if (type == ComponentType::CustomBehaviour)
	{
		for (const auto& name : ScriptRegistry::GetInstance().GetRegisteredScriptNames())
			if (ImGui::Button(("Add " + name).c_str())) created = create({ { "className", name } });
	}
	else if (type == ComponentType::Drawable)
	{
		static int kind = 0, shape = 1, surface = 0;
		static char assetPath[1024] = {};
		ImGui::Combo("Kind", &kind, "Primitive\0Model\0");
		if (kind == 0)
		{
			ImGui::Combo("Shape", &shape, "Cone\0Cube\0Plane\0Prism\0Sphere\0");
			ImGui::Combo("Surface", &surface, "Material\0Textured\0");
			if (ImGui::Button("Add Primitive")) created = create({ { "kind", "primitive" },
				{ "shape", static_cast<Primitive::Shape>(shape) }, { "surface", static_cast<Primitive::SurfaceMode>(surface) } });
		}
		else
		{
			ImGui::InputText("Project-relative model path", assetPath, sizeof(assetPath));
			if (ImGui::Button("Add Model")) created = create({ { "kind", "model" }, { "asset", assetPath } });
		}
	}
	else
	{
		bool exists = false;
		for (const auto& component : owner.GetComponents())
			exists |= component->IsType(type) && !component->IsPendingInspectorRemoval();
		ImGui::BeginDisabled(entry->singlePerObject && exists);
		if (ImGui::Button(("Add " + std::string(ComponentTypeToString(type))).c_str())) created = create(nlohmann::json::object());
		ImGui::EndDisabled();
	}
	if (!error.empty()) ImGui::TextWrapped("%s", error.c_str());
	return created;
}
