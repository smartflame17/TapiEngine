#include "SceneSerializer.h"
#include "ComponentRegistry.h"
#include "JsonRead.h"
#include "SceneFormat.h"
#include "../Scene/GameObject.h"
#include "../SmflmWin.h"
#include <algorithm>
#include <fstream>
#include <unordered_set>

using nlohmann::json;
namespace
{
constexpr std::size_t MaxDepth = 512;
const ComponentRegistry& Registry(const ComponentRegistry* registry)
{
	return registry ? *registry : ComponentRegistry::Builtins();
}
template<typename Fn>
void At(SerializationError diagnostic, const std::string& field, Fn&& operation)
{
	diagnostic.jsonPath += field;
	try { operation(); }
	catch (const SerializationException&) { throw; }
	catch (const MetadataError& error)
	{
		diagnostic.jsonPath += error.field; diagnostic.message = error.what();
		throw SerializationException(std::move(diagnostic));
	}
	catch (const std::exception& error)
	{
		diagnostic.message = error.what(); throw SerializationException(std::move(diagnostic));
	}
}
void Identity(const Guid& id, std::unordered_set<Guid>& ids)
{
	if (id.IsNull()) throw std::invalid_argument("Identity cannot be a null UUID.");
	if (!ids.insert(id).second) throw std::invalid_argument("Duplicate UUID: " + id.ToString());
}
SerializationError ObjectDiagnostic(const json& object, const std::string& path)
{
	SerializationError diagnostic;
	diagnostic.jsonPath = path;
	if (object.is_object() && object.contains("name") && object.at("name").is_string())
		diagnostic.objectName = object.at("name").get<std::string>();
	return diagnostic;
}

void ValidateObject(const json& object, const std::string& path, const SerializationContext& context,
	std::unordered_set<Guid>& ids, std::size_t depth)
{
	auto diagnostic = ObjectDiagnostic(object, path);
	At(diagnostic, "", [&] { JsonRead::Object(object); if (depth > MaxDepth) throw std::invalid_argument("Scene hierarchy is too deep."); });
	At(diagnostic, ".id", [&] { diagnostic.objectId = object.at("id").get<Guid>(); });
	At(diagnostic, ".id", [&] { Identity(*diagnostic.objectId, ids); });
	At(diagnostic, ".name", [&] { object.at("name").get<std::string>(); });
	At(diagnostic, "", [&] { JsonRead::Value(object, "static", false); });
	At(diagnostic, ".transform", [&] { object.at("transform").get<Transform>(); });
	At(diagnostic, ".components", [&] { if (!object.at("components").is_array()) throw std::invalid_argument("Expected a component array."); });
	std::unordered_set<ComponentType> singletons;
	const auto& components = object.at("components");
	for (std::size_t i = 0; i < components.size(); ++i)
	{
		const auto& component = components[i];
		auto detail = diagnostic;
		detail.jsonPath += ".components[" + std::to_string(i) + "]";
		At(detail, "", [&] { JsonRead::Object(component); });
		At(detail, ".id", [&] { detail.componentId = component.at("id").get<Guid>(); });
		At(detail, ".id", [&] { Identity(*detail.componentId, ids); });
		const ComponentRegistration* registration = nullptr;
		At(detail, ".type", [&] {
			detail.componentType = component.at("type").get<std::string>();
		});
		At(detail, ".type", [&] {
			registration = Registry(context.registry).Find(detail.componentType);
			if (!registration) throw std::invalid_argument("Unregistered component type: " + detail.componentType);
			if (!registration->sceneSupported) throw std::invalid_argument("Scene serialization is not implemented for this component type yet.");
			if (registration->singlePerObject && !singletons.insert(registration->type).second)
				throw std::invalid_argument("Only one component of this type is allowed per GameObject.");
		});
		At(detail, ".data", [&] { JsonRead::Object(component.at("data")); registration->validate(component.at("data"), context); });
	}
	At(diagnostic, ".children", [&] { if (!object.at("children").is_array()) throw std::invalid_argument("Expected a child array."); });
	for (std::size_t i = 0; i < object.at("children").size(); ++i)
		ValidateObject(object.at("children")[i], path + ".children[" + std::to_string(i) + "]", context, ids, depth + 1);
}

json SerializeObjectRecursive(const GameObject& object, const std::string& path, const SerializationContext& context,
	std::unordered_set<Guid>& ids, std::size_t depth)
{
	SerializationError diagnostic;
	diagnostic.jsonPath = path; diagnostic.objectName = object.GetName(); diagnostic.objectId = object.GetId();
	At(diagnostic, "", [&] {
		if (depth > MaxDepth) throw std::invalid_argument("Scene hierarchy is too deep.");
		if (object.IsPendingKill()) throw std::invalid_argument("Cannot serialize a destroyed object subtree.");
	});
	At(diagnostic, ".id", [&] { Identity(object.GetId(), ids); });
	json result;
	At(diagnostic, ".transform", [&] {
		result = { { "id", object.GetId() }, { "name", object.GetName() }, { "static", object.IsStatic() },
			{ "transform", object.GetTransform() }, { "components", json::array() }, { "children", json::array() } };
	});
	for (const auto& component : object.GetComponents())
	{
		if (component->IsPendingInspectorRemoval()) continue;
		auto detail = diagnostic;
		detail.jsonPath += ".components[" + std::to_string(result["components"].size()) + "]";
		detail.componentId = component->GetId(); detail.componentType = std::string(component->GetSerializationType());
		const ComponentRegistration* registration = Registry(context.registry).Find(detail.componentType);
		if (detail.componentType.empty())
			if (const auto* entry = Registry(context.registry).Find(component->GetType())) detail.componentType = entry->key;
		At(detail, ".type", [&] {
			if (!registration || !registration->sceneSupported)
				throw std::invalid_argument("This component does not support scene serialization yet (" + std::string(ComponentTypeToString(component->GetType())) + ").");
		});
		At(detail, ".id", [&] { Identity(component->GetId(), ids); });
		json data;
		At(detail, ".data", [&] { component->SerializeData(data, context); JsonRead::Object(data); registration->validate(data, context); });
		result["components"].push_back({ { "id", component->GetId() }, { "type", detail.componentType }, { "data", std::move(data) } });
	}
	for (const auto& child : object.GetChildren())
	{
		if (child->IsPendingKill()) continue;
		const auto childPath = path + ".children[" + std::to_string(result["children"].size()) + "]";
		result["children"].push_back(SerializeObjectRecursive(*child, childPath, context, ids, depth + 1));
	}
	return result;
}

struct ComponentTask
{
	GameObject* owner;
	const json* document;
	const ComponentRegistration* registration;
	SerializationError diagnostic;
	std::size_t serializedIndex;
};
std::unique_ptr<GameObject> CreateObjectRecursive(const json& data, const std::string& path, Scene& scene,
	LoadContext& context, std::vector<ComponentTask>& tasks)
{
	auto object = std::make_unique<GameObject>(scene, data.at("name").get<std::string>());
	object->SetId(data.at("id").get<Guid>());
	object->SetStatic(JsonRead::Value(data, "static", false));
	object->SetTransform(data.at("transform").get<Transform>());
	context.gameObjects.emplace(object->GetId(), object.get());
	for (std::size_t i = 0; i < data.at("components").size(); ++i)
	{
		const auto& component = data.at("components")[i];
		auto diagnostic = ObjectDiagnostic(data, path + ".components[" + std::to_string(i) + "]");
		diagnostic.objectId = object->GetId(); diagnostic.componentId = component.at("id").get<Guid>();
		diagnostic.componentType = component.at("type").get<std::string>();
		tasks.push_back({ object.get(), &component, Registry(context.registry).Find(diagnostic.componentType), std::move(diagnostic), i });
	}
	for (std::size_t i = 0; i < data.at("children").size(); ++i)
		object->AddChild(CreateObjectRecursive(data.at("children")[i], path + ".children[" + std::to_string(i) + "]", scene, context, tasks));
	return object;
}
LoadResult Failure(const std::exception& error, std::string path = "$")
{
	LoadResult result;
	if (const auto* serialization = dynamic_cast<const SerializationException*>(&error)) result.errors.push_back(serialization->diagnostic);
	else { SerializationError diagnostic; diagnostic.jsonPath = std::move(path); diagnostic.message = error.what(); result.errors.push_back(std::move(diagnostic)); }
	return result;
}
}

json SceneSerializer::SerializeGameObject(const GameObject& object, const SerializationContext& context)
{
	std::unordered_set<Guid> ids;
	return SerializeObjectRecursive(object, "$", context, ids, 0);
}

json SceneSerializer::Serialize(const Scene& scene, const SerializationContext& context)
{
	json objects = json::array();
	std::unordered_set<Guid> ids;
	for (const auto& object : scene.GetRootObjects())
	{
		if (object->IsPendingKill()) continue;
		objects.push_back(SerializeObjectRecursive(*object, "$.scene.objects[" + std::to_string(objects.size()) + "]", context, ids, 0));
	}
	return { { "format", SceneFormat::Name }, { "version", SceneFormat::CurrentVersion },
		{ "scene", { { "name", scene.GetName() }, { "objects", std::move(objects) } } } };
}

LoadResult SceneSerializer::Deserialize(const json& document, Scene& scene, LoadContext& context)
{
	try
	{
		SerializationError diagnostic;
		At(diagnostic, "", [&] { JsonRead::Object(document); });
		At(diagnostic, ".format", [&] { if (document.at("format").get<std::string>() != SceneFormat::Name) throw std::invalid_argument("Expected TapiScene format."); });
		At(diagnostic, ".version", [&] {
			const auto& version = document.at("version");
			if (!version.is_number_integer() || version != SceneFormat::CurrentVersion) throw std::invalid_argument("Unsupported scene version.");
		});
		const json* sceneData = nullptr;
		At(diagnostic, ".scene", [&] { sceneData = &document.at("scene"); JsonRead::Object(*sceneData); });
		std::string name;
		At(diagnostic, ".scene.name", [&] { name = sceneData->at("name").get<std::string>(); });
		At(diagnostic, ".scene.objects", [&] { if (!sceneData->at("objects").is_array()) throw std::invalid_argument("Expected a root object array."); });
		SerializationContext validationContext{ context.projectRoot, context.registry };
		std::unordered_set<Guid> ids;
		const auto& objects = sceneData->at("objects");
		for (std::size_t i = 0; i < objects.size(); ++i)
			ValidateObject(objects[i], "$.scene.objects[" + std::to_string(i) + "]", validationContext, ids, 0);

		// Reconstruct a detached hierarchy. A failure releases its resources and
		// registrations while the original hierarchy and identity maps stay intact.
		LoadContext prepared{ context.graphics, context.projectRoot };
		prepared.assets = context.assets; prepared.registry = context.registry;
		prepared.deferScriptRegistration = true;
		LoadResult result;
		std::vector<std::unique_ptr<GameObject>> roots;
		std::vector<ComponentTask> tasks;
		for (std::size_t i = 0; i < objects.size(); ++i)
			roots.push_back(CreateObjectRecursive(objects[i], "$.scene.objects[" + std::to_string(i) + "]", scene, prepared, tasks));
		std::stable_sort(tasks.begin(), tasks.end(), [](const auto& lhs, const auto& rhs) { return lhs.registration->loadOrder < rhs.registration->loadOrder; });
		for (const auto& task : tasks)
			At(task.diagnostic, ".data", [&] {
				prepared.reportWarning = [&](std::string field, std::string message) {
					auto warning = task.diagnostic;
					warning.jsonPath += ".data" + field; warning.message = std::move(message);
					result.warnings.push_back(std::move(warning));
				};
				auto& component = Registry(prepared.registry).Create(task.diagnostic.componentType, *task.owner, task.document->at("data"), prepared);
				component.SetId(*task.diagnostic.componentId);
				prepared.components.emplace(component.GetId(), &component);
			});
		// Dependency order is only for construction; retain the authored inspector order.
		for (const auto& [id, object] : prepared.gameObjects)
		{
			std::unordered_map<Guid, std::size_t> order;
			for (const auto& task : tasks) if (task.owner == object) order.emplace(*task.diagnostic.componentId, task.serializedIndex);
			std::stable_sort(object->components.begin(), object->components.end(), [&](const auto& lhs, const auto& rhs) { return order.at(lhs->GetId()) < order.at(rhs->GetId()); });
		}

		// The skybox is currently a fixed editor environment, not scene metadata.
		auto skybox = std::move(scene.skybox);
		scene.Clear();
		scene.skybox = std::move(skybox);
		scene.SetName(std::move(name)); scene.rootObjects = std::move(roots);
		for (const auto& task : tasks)
		{
			auto* component = prepared.components.at(*task.diagnostic.componentId);
			if (auto* drawable = dynamic_cast<DrawableComponent*>(component)) scene.RegisterDrawable(drawable);
			if (auto* script = dynamic_cast<CustomBehaviour*>(component)) scene.RegisterScript(*script);
		}
		context.gameObjects.swap(prepared.gameObjects); context.components.swap(prepared.components);
		return result;
	}
	catch (const std::exception& error) { return Failure(error); }
}

LoadResult SceneSerializer::SaveToFile(const Scene& scene, const std::filesystem::path& path, const SerializationContext& context)
{
	std::filesystem::path temporary;
	try
	{
		const auto document = Serialize(scene, context);
		temporary = path; temporary += L".tmp." + std::filesystem::path(Guid::Generate().ToString()).wstring();
		std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
		if (!file) throw std::runtime_error("Cannot open scene temporary file: " + temporary.generic_string());
		file << document.dump(2) << '\n'; file.flush();
		if (!file) throw std::runtime_error("Cannot write scene temporary file.");
		file.close(); if (!file) throw std::runtime_error("Cannot close scene temporary file.");
		if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
			throw std::runtime_error("Cannot replace scene file (Win32 error " + std::to_string(GetLastError()) + ").");
		return {};
	}
	catch (const std::exception& error)
	{
		if (!temporary.empty()) { std::error_code ignored; std::filesystem::remove(temporary, ignored); }
		return Failure(error);
	}
}

LoadResult SceneSerializer::LoadFromFile(const std::filesystem::path& path, Scene& scene, LoadContext& context)
{
	try
	{
		std::ifstream file(path, std::ios::binary);
		if (!file) throw std::runtime_error("Cannot open scene file: " + path.generic_string());
		const auto document = json::parse(file);
		if (file.bad()) throw std::runtime_error("Cannot read scene file: " + path.generic_string());
		return Deserialize(document, scene, context);
	}
	catch (const std::exception& error) { return Failure(error); }
}
