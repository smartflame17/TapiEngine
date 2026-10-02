#pragma once

#include "Json.h"
#include "../Components/CustomBehaviour.h"
#include "../Components/Rigidbody.h"
#include "../Components/Collider.h"
#include "../Graphics/Drawable/Primitive.h"
#include "../Graphics/Lighting/RenderLight.h"
#include "../Graphics/Animation/Animation.h"
#include <stdexcept>
#include <utility>

namespace JsonDetail
{
	template<typename Enum> struct EnumMapping;

	template<> struct EnumMapping<ComponentType>
	{
		inline static constexpr std::pair<ComponentType, std::string_view> values[] = {
			{ ComponentType::Drawable, "tapi.drawable" }, { ComponentType::CustomBehaviour, "tapi.script" },
			{ ComponentType::SpotLight, "tapi.spot_light" }, { ComponentType::PointLight, "tapi.point_light" },
			{ ComponentType::DirectionalLight, "tapi.directional_light" }, { ComponentType::Camera, "tapi.camera" },
			{ ComponentType::Other, "tapi.other" }, { ComponentType::Animator, "tapi.animator" },
			{ ComponentType::Rigidbody, "tapi.rigidbody" }, { ComponentType::Collider, "tapi.collider" },
			{ ComponentType::AudioClip, "tapi.audio_clip" }
		};
	};
	template<> struct EnumMapping<PropertyType>
	{
		inline static constexpr std::pair<PropertyType, std::string_view> values[] = {
			{ PropertyType::Int, "int" }, { PropertyType::Float, "float" }, { PropertyType::String, "string" },
			{ PropertyType::Vector3, "vector3" }, { PropertyType::Color, "color" }, { PropertyType::Bool, "bool" }
		};
	};
	template<> struct EnumMapping<Rigidbody::Type>
	{
		inline static constexpr std::pair<Rigidbody::Type, std::string_view> values[] = {
			{ Rigidbody::Type::Static, "static" }, { Rigidbody::Type::Dynamic, "dynamic" }
		};
	};
	template<> struct EnumMapping<Collider::Shape>
	{
		inline static constexpr std::pair<Collider::Shape, std::string_view> values[] = {
			{ Collider::Shape::Box, "box" }, { Collider::Shape::Sphere, "sphere" }, { Collider::Shape::Capsule, "capsule" }
		};
	};
	template<> struct EnumMapping<Primitive::Shape>
	{
		inline static constexpr std::pair<Primitive::Shape, std::string_view> values[] = {
			{ Primitive::Shape::Cone, "cone" }, { Primitive::Shape::Cube, "cube" }, { Primitive::Shape::Plane, "plane" },
			{ Primitive::Shape::Prism, "prism" }, { Primitive::Shape::Sphere, "sphere" }
		};
	};
	template<> struct EnumMapping<Primitive::SurfaceMode>
	{
		inline static constexpr std::pair<Primitive::SurfaceMode, std::string_view> values[] = {
			{ Primitive::SurfaceMode::Material, "material" }, { Primitive::SurfaceMode::Textured, "textured" }
		};
	};
	template<> struct EnumMapping<LightType>
	{
		inline static constexpr std::pair<LightType, std::string_view> values[] = {
			{ LightType::None, "none" }, { LightType::Directional, "directional" },
			{ LightType::Point, "point" }, { LightType::Spot, "spot" }
		};
	};
	template<> struct EnumMapping<Animation::PlaybackState>
	{
		inline static constexpr std::pair<Animation::PlaybackState, std::string_view> values[] = {
			{ Animation::PlaybackState::Stopped, "stopped" }, { Animation::PlaybackState::Playing, "playing" },
			{ Animation::PlaybackState::Paused, "paused" }, { Animation::PlaybackState::Completed, "completed" }
		};
	};

	template<typename Enum>
	void EnumToJson(nlohmann::json& out, Enum value)
	{
		for (const auto& [candidate, name] : EnumMapping<Enum>::values)
			if (candidate == value) { out = std::string(name); return; }
		throw std::invalid_argument("Cannot serialize an invalid enum value.");
	}
	template<typename Enum>
	void EnumFromJson(const nlohmann::json& in, Enum& value)
	{
		const auto& name = in.get_ref<const std::string&>();
		for (const auto& [candidate, key] : EnumMapping<Enum>::values)
			if (key == name) { value = candidate; return; }
		throw std::invalid_argument("Unknown enum value: " + name);
	}
}

// Unlike nlohmann's default enum mapping, unknown strings are errors rather
// than silently becoming the first enum value. Declare each overload for ADL.
inline void to_json(nlohmann::json& out, ComponentType value) { JsonDetail::EnumToJson(out, value); }
inline void from_json(const nlohmann::json& in, ComponentType& value) { JsonDetail::EnumFromJson(in, value); }
inline void to_json(nlohmann::json& out, PropertyType value) { JsonDetail::EnumToJson(out, value); }
inline void from_json(const nlohmann::json& in, PropertyType& value) { JsonDetail::EnumFromJson(in, value); }
inline void to_json(nlohmann::json& out, Rigidbody::Type value) { JsonDetail::EnumToJson(out, value); }
inline void from_json(const nlohmann::json& in, Rigidbody::Type& value) { JsonDetail::EnumFromJson(in, value); }
inline void to_json(nlohmann::json& out, Collider::Shape value) { JsonDetail::EnumToJson(out, value); }
inline void from_json(const nlohmann::json& in, Collider::Shape& value) { JsonDetail::EnumFromJson(in, value); }
inline void to_json(nlohmann::json& out, Primitive::Shape value) { JsonDetail::EnumToJson(out, value); }
inline void from_json(const nlohmann::json& in, Primitive::Shape& value) { JsonDetail::EnumFromJson(in, value); }
inline void to_json(nlohmann::json& out, Primitive::SurfaceMode value) { JsonDetail::EnumToJson(out, value); }
inline void from_json(const nlohmann::json& in, Primitive::SurfaceMode& value) { JsonDetail::EnumFromJson(in, value); }
inline void to_json(nlohmann::json& out, LightType value) { JsonDetail::EnumToJson(out, value); }
inline void from_json(const nlohmann::json& in, LightType& value) { JsonDetail::EnumFromJson(in, value); }
namespace Animation
{
	inline void to_json(nlohmann::json& out, PlaybackState value) { JsonDetail::EnumToJson(out, value); }
	inline void from_json(const nlohmann::json& in, PlaybackState& value) { JsonDetail::EnumFromJson(in, value); }
}
