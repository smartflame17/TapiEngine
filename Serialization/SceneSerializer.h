#pragma once
#include "Json.h"
#include "SerializationContext.h"
#include "SerializationError.h"

class Scene;
class GameObject;

class SceneSerializer
{
public:
	static nlohmann::json Serialize(const Scene& scene, const SerializationContext& context = {});
	// Public recursive subtree writer. Scene export and future prefab export use
	// this same object schema, with no scene envelope required for a subtree.
	static nlohmann::json SerializeGameObject(const GameObject& object, const SerializationContext& context = {});
	static LoadResult Deserialize(const nlohmann::json& document, Scene& scene, LoadContext& context);
	static LoadResult SaveToFile(const Scene& scene, const std::filesystem::path& path, const SerializationContext& context = {});
	static LoadResult LoadFromFile(const std::filesystem::path& path, Scene& scene, LoadContext& context);
};
