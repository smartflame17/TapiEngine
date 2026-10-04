#include "ComponentRegistry.h"
#include "JsonEnums.h"
#include "JsonRead.h"
#include "../Scene/GameObject.h"
#include "../Graphics/Camera.h"
#include "../Graphics/Lighting/PointLight.h"
#include "../Graphics/Lighting/SpotLight.h"
#include "../Graphics/Lighting/DirectionalLight.h"
#include "../Components/AudioClip.h"

using nlohmann::json;
namespace
{
void ValidateCamera(const json& data, const SerializationContext&)
{
	JsonRead::Object(data);
	JsonRead::Nonnegative(data, "speed", 0.1f);
	JsonRead::Nonnegative(data, "sensitivity", 0.004f);
}
void ValidateLight(const json& data, const SerializationContext&)
{
	JsonRead::Object(data);
	JsonRead::Value<DirectX::XMFLOAT3>(data, "color", { 1, 1, 1 });
	JsonRead::Nonnegative(data, "intensity", 1);
}
void ValidatePoint(const json& data, const SerializationContext& context)
{
	ValidateLight(data, context);
	JsonRead::Nonnegative(data, "gizmoRadius", 0.5f, true);
	JsonRead::Nonnegative(data, "attConstant", 1);
	JsonRead::Nonnegative(data, "attLinear", 0.045f);
	JsonRead::Nonnegative(data, "attQuadratic", 0.0075f);
}
void ValidateSpot(const json& data, const SerializationContext& context)
{
	ValidatePoint(data, context);
	const float inner = JsonRead::Float(data, "innerAngle", DirectX::XMConvertToRadians(20));
	const float outer = JsonRead::Float(data, "outerAngle", DirectX::XMConvertToRadians(30));
	if (inner < 0.0174533f || inner > DirectX::XM_PIDIV2 - 0.0174533f)
		throw MetadataError(".innerAngle", "Spot cone angle is outside the supported range.");
	if (outer < inner || outer > DirectX::XM_PIDIV2 - 0.0174533f)
		throw MetadataError(".outerAngle", "Outer cone must be at least the inner cone and below 89 degrees.");
}
void ValidateRigidbody(const json& data, const SerializationContext&)
{
	JsonRead::Object(data);
	JsonRead::Value(data, "bodyType", Rigidbody::Type::Static);
	JsonRead::Float(data, "gravityScale", 1);
	JsonRead::Nonnegative(data, "linearDamping", 0);
	JsonRead::Nonnegative(data, "angularDamping", 0);
	if (data.contains("motionLocks"))
	{
		const auto& locks = data.at("motionLocks");
		try
		{
			JsonRead::Object(locks);
			for (const auto* key : { "linearX", "linearY", "linearZ", "angularX", "angularY", "angularZ" })
				JsonRead::Value(locks, key, false);
		}
		catch (const MetadataError& error) { throw MetadataError(".motionLocks" + error.field, error.what()); }
	}
}
void ValidateCollider(const json& data, const SerializationContext&)
{
	JsonRead::Object(data);
	JsonRead::Value(data, "shape", Collider::Shape::Box);
	JsonRead::Value<DirectX::XMFLOAT3>(data, "center", { 0, 0, 0 });
	const auto size = JsonRead::Value<DirectX::XMFLOAT3>(data, "size", { 1, 1, 1 });
	if (size.x <= 0 || size.y <= 0 || size.z <= 0) throw MetadataError(".size", "Collider dimensions must be positive.");
	JsonRead::Nonnegative(data, "radius", 0.5f, true);
	JsonRead::Nonnegative(data, "height", 2, true);
	JsonRead::Nonnegative(data, "density", 1, true);
	JsonRead::Nonnegative(data, "friction", 0);
	if (JsonRead::Nonnegative(data, "restitution", 0) > 1) throw MetadataError(".restitution", "Restitution must be between zero and one.");
}
template<typename T> Component& Create(GameObject& owner, const json& data, LoadContext& context)
{
	T& component = owner.AddComponent<T>();
	component.DeserializeData(data, context);
	return component;
}
template<typename T> Component& CreateLight(GameObject& owner, const json& data, LoadContext& context)
{
	T& component = owner.AddComponent<T>(context.graphics);
	component.DeserializeData(data, context);
	return component;
}
}

void Camera::SerializeData(json& out, const SerializationContext&) const
{
	out = { { "speed", camSpeed }, { "sensitivity", rotateSpeed } };
}
void Camera::DeserializeData(const json& data, LoadContext&)
{
	ValidateCamera(data, {});
	camSpeed = JsonRead::Float(data, "speed", camSpeed);
	rotateSpeed = JsonRead::Float(data, "sensitivity", rotateSpeed);
}
void PointLight::SerializeData(json& out, const SerializationContext&) const
{
	out = { { "color", diffuseColor }, { "intensity", diffuseIntensity }, { "gizmoRadius", gizmoRadius },
		{ "attConstant", attConst }, { "attLinear", attLinear }, { "attQuadratic", attQuad } };
}
void PointLight::DeserializeData(const json& data, LoadContext&)
{
	ValidatePoint(data, {});
	diffuseColor = JsonRead::Value(data, "color", diffuseColor);
	diffuseIntensity = JsonRead::Float(data, "intensity", diffuseIntensity);
	SetAttenuation(JsonRead::Float(data, "attConstant", attConst), JsonRead::Float(data, "attLinear", attLinear), JsonRead::Float(data, "attQuadratic", attQuad));
}
void SpotLight::SerializeData(json& out, const SerializationContext&) const
{
	out = { { "color", color }, { "intensity", intensity }, { "gizmoRadius", gizmoRadius },
		{ "attConstant", attConst }, { "attLinear", attLinear }, { "attQuadratic", attQuad },
		{ "innerAngle", innerAngle }, { "outerAngle", outerAngle } };
}
void SpotLight::DeserializeData(const json& data, LoadContext&)
{
	ValidateSpot(data, {});
	color = JsonRead::Value(data, "color", color);
	intensity = JsonRead::Float(data, "intensity", intensity);
	SetAttenuation(JsonRead::Float(data, "attConstant", attConst), JsonRead::Float(data, "attLinear", attLinear), JsonRead::Float(data, "attQuadratic", attQuad));
	SetConeAngles(JsonRead::Float(data, "innerAngle", innerAngle), JsonRead::Float(data, "outerAngle", outerAngle));
}
void DirectionalLight::SerializeData(json& out, const SerializationContext&) const
{
	out = { { "color", color }, { "intensity", intensity } };
}
void DirectionalLight::DeserializeData(const json& data, LoadContext&)
{
	ValidateLight(data, {});
	color = JsonRead::Value(data, "color", color);
	intensity = JsonRead::Float(data, "intensity", intensity);
}
void Rigidbody::SerializeData(json& out, const SerializationContext&) const
{
	out = { { "bodyType", bodyType }, { "gravityScale", gravityScale }, { "linearDamping", linearDamping }, { "angularDamping", angularDamping },
		{ "motionLocks", { { "linearX", motionLocks.linearX }, { "linearY", motionLocks.linearY }, { "linearZ", motionLocks.linearZ },
			{ "angularX", motionLocks.angularX }, { "angularY", motionLocks.angularY }, { "angularZ", motionLocks.angularZ } } } };
}
void Rigidbody::DeserializeData(const json& data, LoadContext&)
{
	ValidateRigidbody(data, {});
	SetType(JsonRead::Value(data, "bodyType", bodyType));
	SetGravityScale(JsonRead::Float(data, "gravityScale", gravityScale));
	SetLinearDamping(JsonRead::Float(data, "linearDamping", linearDamping));
	SetAngularDamping(JsonRead::Float(data, "angularDamping", angularDamping));
	if (data.contains("motionLocks"))
	{
		const auto& locks = data.at("motionLocks");
		auto restored = motionLocks;
		restored.linearX = JsonRead::Value(locks, "linearX", restored.linearX);
		restored.linearY = JsonRead::Value(locks, "linearY", restored.linearY);
		restored.linearZ = JsonRead::Value(locks, "linearZ", restored.linearZ);
		restored.angularX = JsonRead::Value(locks, "angularX", restored.angularX);
		restored.angularY = JsonRead::Value(locks, "angularY", restored.angularY);
		restored.angularZ = JsonRead::Value(locks, "angularZ", restored.angularZ);
		SetMotionLocks(restored);
	}
}
void Collider::SerializeData(json& out, const SerializationContext&) const
{
	out = { { "shape", geometry.shape }, { "center", geometry.center }, { "size", geometry.size },
		{ "radius", geometry.radius }, { "height", geometry.height }, { "density", density }, { "friction", friction }, { "restitution", restitution } };
}
void Collider::DeserializeData(const json& data, LoadContext&)
{
	ValidateCollider(data, {});
	auto restored = geometry;
	restored.shape = JsonRead::Value(data, "shape", restored.shape);
	restored.center = JsonRead::Value(data, "center", restored.center);
	restored.size = JsonRead::Value(data, "size", restored.size);
	restored.radius = JsonRead::Float(data, "radius", restored.radius);
	restored.height = JsonRead::Float(data, "height", restored.height);
	SetGeometry(restored);
	SetDensity(JsonRead::Float(data, "density", density));
	SetFriction(JsonRead::Float(data, "friction", friction));
	SetRestitution(JsonRead::Float(data, "restitution", restitution));
}

void RegisterResourceComponents(ComponentRegistry& registry);
void RegisterBuiltinComponents(ComponentRegistry& registry)
{
	registry.Register({ std::string(Camera::SerializationType), ComponentType::Camera, 10, false, true, Create<Camera>, ValidateCamera });
	registry.Register({ std::string(PointLight::SerializationType), ComponentType::PointLight, 10, false, true,
		[](GameObject& owner, const json& data, LoadContext& context) -> Component& {
			auto& light = owner.AddComponent<PointLight>(context.graphics, JsonRead::Float(data, "gizmoRadius", 0.5f));
			light.DeserializeData(data, context); return light;
		}, ValidatePoint });
	registry.Register({ std::string(SpotLight::SerializationType), ComponentType::SpotLight, 10, false, true,
		[](GameObject& owner, const json& data, LoadContext& context) -> Component& {
			auto& light = owner.AddComponent<SpotLight>(context.graphics, JsonRead::Float(data, "gizmoRadius", 0.4f));
			light.DeserializeData(data, context); return light;
		}, ValidateSpot });
	registry.Register({ std::string(DirectionalLight::SerializationType), ComponentType::DirectionalLight, 10, false, true, CreateLight<DirectionalLight>, ValidateLight });
	registry.Register({ std::string(Rigidbody::SerializationType), ComponentType::Rigidbody, 20, true, true, Create<Rigidbody>, ValidateRigidbody });
	registry.Register({ std::string(Collider::SerializationType), ComponentType::Collider, 30, true, true, Create<Collider>, ValidateCollider });
	RegisterResourceComponents(registry);
	// Retain existing editor functionality without silently exporting unsupported payloads.
	registry.Register({ "tapi.audio_clip", ComponentType::AudioClip, 10, false, false,
		[](GameObject& owner, const json&, LoadContext&) -> Component& { return owner.AddComponent<AudioClip>(); }, {} });
	registry.Register({ "tapi.script", ComponentType::CustomBehaviour, 50, false, true,
		[](GameObject& owner, const json& data, LoadContext& context) -> Component& {
			auto* script = owner.AddScript(data.at("className").get<std::string>(), !context.deferScriptRegistration);
			if (!script) throw std::runtime_error("Script factory returned no component.");
			script->DeserializeData(data, context);
			return *script;
		}, CustomBehaviour::ValidateSerializedData });
}
