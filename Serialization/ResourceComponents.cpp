#include "ComponentRegistry.h"
#include "JsonEnums.h"
#include "JsonRead.h"
#include "../Scene/GameObject.h"
#include "../Components/Animator.h"
#include "../Graphics/Drawable/Model.h"
#include <unordered_set>

using nlohmann::json;
namespace
{
json MaterialData(const PhongMaterial& value)
{
	return { { "color", value.color }, { "specularColor", value.specularColor },
		{ "specularIntensity", value.specularIntensity }, { "specularPower", value.specularPower } };
}
PhongMaterial ReadMaterial(const json& data, PhongMaterial defaults = {})
{
	JsonRead::Object(data);
	defaults.color = JsonRead::Value(data, "color", defaults.color);
	defaults.specularColor = JsonRead::Value(data, "specularColor", defaults.specularColor);
	defaults.specularIntensity = JsonRead::Nonnegative(data, "specularIntensity", defaults.specularIntensity);
	defaults.specularPower = JsonRead::Nonnegative(data, "specularPower", defaults.specularPower, true);
	return defaults;
}
void ValidateSurface(const json& data, const SerializationContext& context)
{
	JsonRead::Path(data, "texture", context.projectRoot);
	JsonRead::Path(data, "normalMap", context.projectRoot);
	JsonRead::Value(data, "normalMapEnabled", false);
	JsonRead::Value(data, "sampler", Sampler::Type::LinearWrap);
	if (data.contains("material"))
		try { ReadMaterial(data.at("material")); }
		catch (const MetadataError& error) { throw MetadataError(".material" + error.field, error.what()); }
}
template<typename Fn>
void ForArray(const json& data, const char* key, Fn&& visit)
{
	if (!data.contains(key)) return;
	const auto& array = data.at(key);
	if (!array.is_array()) throw MetadataError("." + std::string(key), "Expected an array.");
	for (std::size_t i = 0; i < array.size(); ++i)
	{
		try { JsonRead::Object(array[i]); visit(array[i]); }
		catch (const MetadataError& error) { throw MetadataError("." + std::string(key) + "[" + std::to_string(i) + "]" + error.field, error.what()); }
	}
}
void ValidateDrawable(const json& data, const SerializationContext& context)
{
	JsonRead::Object(data);
	const auto kind = JsonRead::Value<std::string>(data, "kind", "primitive");
	JsonRead::Value(data, "localTransform", Transform{});
	if (kind == "primitive")
	{
		JsonRead::Value(data, "shape", Primitive::Shape::Cube);
		JsonRead::Value(data, "surface", Primitive::SurfaceMode::Material);
		ValidateSurface(data, context);
	}
	else if (kind == "model")
	{
		JsonRead::Path(data, "asset", context.projectRoot, true);
		std::unordered_set<std::size_t> nodes, meshes;
		ForArray(data, "nodeOverrides", [&](const json& entry) {
			const auto index = JsonRead::Index(entry, "node", (std::numeric_limits<std::size_t>::max)());
			if (index == (std::numeric_limits<std::size_t>::max)() || !nodes.insert(index).second)
				throw MetadataError(".node", "Expected a unique node index.");
			JsonRead::Value(entry, "transform", Transform{});
		});
		ForArray(data, "meshOverrides", [&](const json& entry) {
			const auto index = JsonRead::Index(entry, "instance", (std::numeric_limits<std::size_t>::max)());
			if (index == (std::numeric_limits<std::size_t>::max)() || !meshes.insert(index).second)
				throw MetadataError(".instance", "Expected a unique mesh instance index.");
			ValidateSurface(entry, context);
		});
	}
	else throw MetadataError(".kind", "Unsupported drawable kind: " + kind);
}
void ValidateAnimator(const json& data, const SerializationContext& context)
{
	JsonRead::Object(data);
	JsonRead::Nonnegative(data, "speed", 1, true);
	std::unordered_set<std::string> references;
	ForArray(data, "clips", [&](const json& entry) {
		const auto source = JsonRead::Path(entry, "source", context.projectRoot, true);
		const auto index = JsonRead::Index(entry, "clip", 0);
		JsonRead::Value(entry, "loop", true);
		const auto resolved = AssetPath::ToUtf8(AssetPath::Resolve(source, context.projectRoot));
		if (!references.insert(resolved + "#" + std::to_string(index)).second)
			throw MetadataError(".source", "The same source clip is referenced more than once.");
	});
	const auto selected = JsonRead::Index(data, "selectedClip", 0);
	const auto count = data.contains("clips") ? data.at("clips").size() : 0;
	if ((count == 0 && selected != 0) || (count != 0 && selected >= count))
		throw MetadataError(".selectedClip", "Selected clip index is outside the clip list.");
}

std::filesystem::path ResolveOptional(const std::string& reference, const LoadContext& context)
{
	return reference.empty() ? std::filesystem::path{} : AssetPath::Resolve(reference, context.projectRoot);
}
std::shared_ptr<const TextureAsset> ResolveTexture(const std::string& reference, LoadContext& context, const char* field)
{
	if (reference.empty()) return {};
	try
	{
		auto resource = context.GetAssetLoader().LoadTexture(AssetPath::Resolve(reference, context.projectRoot));
		if (!resource) throw std::runtime_error("Texture loader returned a null resource.");
		return resource;
	}
	catch (const std::exception& error) { throw MetadataError("." + std::string(field), error.what()); }
}
json SurfaceData(const PhongMaterial& material, const std::string& texture, const std::string& normal,
	bool enabled, Sampler::Type sampler, const SerializationContext& context)
{
	return { { "material", MaterialData(material) }, { "texture", AssetPath::Reference(AssetPath::FromUtf8(texture), context.projectRoot) },
		{ "normalMap", AssetPath::Reference(AssetPath::FromUtf8(normal), context.projectRoot) },
		{ "normalMapEnabled", enabled }, { "sampler", sampler } };
}
std::unique_ptr<Drawable> ReconstructDrawable(const json& data, LoadContext& context)
{
	ValidateDrawable(data, { context.projectRoot, context.registry });
	const auto kind = JsonRead::Value<std::string>(data, "kind", "primitive");
	std::unique_ptr<Drawable> result;
	if (kind == "model")
	{
		const auto source = AssetPath::Resolve(data.at("asset").get<std::string>(), context.projectRoot);
		std::shared_ptr<const ModelAsset> asset;
		try { asset = context.GetAssetLoader().LoadModel(source); }
		catch (const std::exception& error) { throw MetadataError(".asset", error.what()); }
		if (!asset) throw MetadataError(".asset", "Model loader returned a null resource.");
		auto model = std::make_unique<Model>(context.graphics, std::move(asset), source, &context.GetAssetLoader());
		ForArray(data, "nodeOverrides", [&](const json& entry) {
			const auto index = JsonRead::Index(entry, "node", 0);
			if (index >= model->GetNodeTransforms().size()) throw MetadataError(".node", "Node index does not exist in the imported model.");
			model->SetNodeTransform(index, JsonRead::Value(entry, "transform", Transform{}));
		});
		ForArray(data, "meshOverrides", [&](const json& entry) {
			const auto index = JsonRead::Index(entry, "instance", 0);
			if (index >= model->GetMeshInstanceCount()) throw MetadataError(".instance", "Mesh instance does not exist in the imported model.");
			auto& mesh = model->GetMeshInstance(index);
			if (entry.contains("material")) mesh.SetMaterial(ReadMaterial(entry.at("material"), mesh.GetMaterial()));
			const auto texture = JsonRead::Value(entry, "texture", AssetPath::Reference(AssetPath::FromUtf8(mesh.GetTexturePath()), context.projectRoot));
			const auto normal = JsonRead::Value(entry, "normalMap", AssetPath::Reference(AssetPath::FromUtf8(mesh.GetNormalMapPath()), context.projectRoot));
			auto textureAsset = ResolveTexture(texture, context, "texture");
			auto normalAsset = ResolveTexture(normal, context, "normalMap");
			mesh.SetResources(context.graphics, AssetPath::ToUtf8(ResolveOptional(texture, context)), std::move(textureAsset),
				AssetPath::ToUtf8(ResolveOptional(normal, context)), std::move(normalAsset), JsonRead::Value(entry, "normalMapEnabled", mesh.IsNormalMapEnabled()));
			mesh.SetSamplerType(context.graphics, JsonRead::Value(entry, "sampler", mesh.GetSamplerType()));
		});
		result = std::move(model);
	}
	else
	{
		const auto shape = JsonRead::Value(data, "shape", Primitive::Shape::Cube);
		const auto surface = JsonRead::Value(data, "surface", Primitive::SurfaceMode::Material);
		const auto texture = JsonRead::Value<std::string>(data, "texture", "");
		const auto normal = JsonRead::Value<std::string>(data, "normalMap", "");
		// Resolve resource data explicitly, then pass it to the reconstruction API.
		auto textureAsset = ResolveTexture(texture, context, "texture");
		auto normalAsset = ResolveTexture(normal, context, "normalMap");
		auto primitive = std::make_unique<Primitive>(context.graphics, shape, surface);
		if (data.contains("material")) primitive->SetMaterial(ReadMaterial(data.at("material")));
		primitive->SetResources(AssetPath::ToUtf8(ResolveOptional(texture, context)), std::move(textureAsset),
			AssetPath::ToUtf8(ResolveOptional(normal, context)), std::move(normalAsset), JsonRead::Value(data, "normalMapEnabled", !normal.empty()));
		primitive->SetSamplerType(JsonRead::Value(data, "sampler", Sampler::Type::LinearWrap));
		result = std::move(primitive);
	}
	result->SetTransform(JsonRead::Value(data, "localTransform", Transform{}));
	return result;
}
}

void DrawableComponent::SerializeData(json& out, const SerializationContext& context) const
{
	if (const auto* primitive = dynamic_cast<const Primitive*>(drawable.get()))
	{
		out = SurfaceData(primitive->GetMaterial(), primitive->GetTexturePath(), primitive->GetNormalMapPath(),
			primitive->IsNormalMapEnabled(), primitive->GetSamplerType(), context);
		out["kind"] = "primitive"; out["shape"] = primitive->GetShape(); out["surface"] = primitive->GetSurfaceMode();
	}
	else if (const auto* model = dynamic_cast<const Model*>(drawable.get()))
	{
		if (model->GetSourcePath().empty()) throw MetadataError(".asset", "A model needs an asset reference to be saved; in-memory geometry is not serialized.");
		out = { { "kind", "model" }, { "asset", AssetPath::Reference(model->GetSourcePath(), context.projectRoot) },
			{ "nodeOverrides", json::array() }, { "meshOverrides", json::array() } };
		const auto& transforms = model->GetNodeTransforms();
		for (std::size_t i = 0; i < transforms.size(); ++i)
			if (json(transforms[i]) != json(Transform{})) out["nodeOverrides"].push_back({ { "node", i }, { "transform", transforms[i] } });
		for (std::size_t i = 0; i < model->GetMeshInstanceCount(); ++i)
		{
			const auto& mesh = model->GetMeshInstance(i);
			const auto& imported = model->GetMeshSource(i);
			if (MaterialData(mesh.GetMaterial()) != MaterialData(imported.material) ||
				AssetPath::FromUtf8(mesh.GetTexturePath()) != imported.baseColorTexture ||
				AssetPath::FromUtf8(mesh.GetNormalMapPath()) != imported.normalTexture ||
				mesh.IsNormalMapEnabled() != !imported.normalTexture.empty() || mesh.GetSamplerType() != Sampler::Type::LinearWrap)
			{
				auto entry = SurfaceData(mesh.GetMaterial(), mesh.GetTexturePath(), mesh.GetNormalMapPath(), mesh.IsNormalMapEnabled(), mesh.GetSamplerType(), context);
				entry["instance"] = i; out["meshOverrides"].push_back(std::move(entry));
			}
		}
	}
	else throw std::invalid_argument("Only Model and Primitive drawables support scene serialization.");
	out["localTransform"] = drawable->GetTransform();
}
void DrawableComponent::DeserializeData(const json& data, LoadContext& context)
{
	drawable = ReconstructDrawable(data, context);
}
void Animator::SerializeData(json& out, const SerializationContext& context) const
{
	out = { { "clips", json::array() }, { "selectedClip", selected }, { "speed", clock.speed } };
	for (std::size_t i = 0; i < clips.size(); ++i)
	{
		const auto& entry = clips[i];
		if (entry.sourcePath.empty()) throw MetadataError(".clips[" + std::to_string(i) + "].source", "Animation clips need a source reference; imported tracks are not serialized.");
		out["clips"].push_back({ { "source", AssetPath::Reference(entry.sourcePath, context.projectRoot) }, { "clip", entry.sourceClip }, { "loop", entry.loop } });
	}
}
void Animator::DeserializeData(const json& data, LoadContext& context)
{
	ValidateAnimator(data, { context.projectRoot, context.registry });
	std::unordered_map<std::string, std::vector<std::shared_ptr<const Animation::AnimationClip>>> loaded;
	while (!clips.empty()) RemoveClip(0);
	ForArray(data, "clips", [&](const json& entry) {
		const auto source = AssetPath::Resolve(entry.at("source").get<std::string>(), context.projectRoot);
		const auto index = JsonRead::Index(entry, "clip", 0);
		const auto key = AssetPath::ToUtf8(source);
		if (!loaded.contains(key))
		{
			try { loaded.emplace(key, context.GetAssetLoader().LoadAnimations(source)); }
			catch (const std::exception& error) { throw MetadataError(".source", error.what()); }
		}
		const auto& resource = loaded.at(key);
		if (index >= resource.size()) throw MetadataError(".clip", "Source clip index does not exist in the imported animation.");
		const auto before = clips.size();
		if (!AddClip(resource[index], source, index, JsonRead::Value(entry, "loop", true))) throw MetadataError(".source", GetStatus());
		if (clips.size() != before + 1) throw MetadataError(".source", "Animation loader returned a duplicate clip identity.");
	});
	SelectClip(JsonRead::Index(data, "selectedClip", 0));
	SetSpeed(JsonRead::Float(data, "speed", 1));
	Stop(); // Pose, playback time and runtime playback state are rebuilt from defaults.
}

void RegisterResourceComponents(ComponentRegistry& registry)
{
	registry.Register({ std::string(DrawableComponent::SerializationType), ComponentType::Drawable, 0, false, true,
		[](GameObject& owner, const json& data, LoadContext& context) -> Component& {
			return owner.AddComponent<DrawableComponent>(ReconstructDrawable(data, context));
		}, ValidateDrawable });
	registry.Register({ std::string(Animator::SerializationType), ComponentType::Animator, 40, true, true,
		[](GameObject& owner, const json& data, LoadContext& context) -> Component& {
			auto& animator = owner.AddComponent<Animator>(); animator.DeserializeData(data, context); return animator;
		}, ValidateAnimator });
}
