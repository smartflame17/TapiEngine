#include "JsonRead.h"
#include "SerializationContext.h"
#include "../Components/CustomBehaviour.h"
#include <limits>
#include <unordered_set>

using nlohmann::json;
namespace
{
const char* TypeName(PropertyType type)
{
	switch (type)
	{
	case PropertyType::Int: return "int";
	case PropertyType::Float: return "float";
	case PropertyType::String: return "string";
	case PropertyType::Vector3: return "vector3";
	case PropertyType::Color: return "color";
	case PropertyType::Bool: return "bool";
	}
	throw std::invalid_argument("Unsupported exposed property type.");
}
std::string FieldPath(const std::string& name) { return ".fields[" + json(name).dump() + "]"; }
void ValidateValue(const std::string& type, const json& value)
{
	if (type == "int")
	{
		if (!value.is_number_integer() ||
			(value.is_number_unsigned() ? value.get<std::uint64_t>() > static_cast<std::uint64_t>((std::numeric_limits<int>::max)()) :
			value.get<std::int64_t>() < (std::numeric_limits<int>::min)() || value.get<std::int64_t>() > (std::numeric_limits<int>::max)()))
			throw std::invalid_argument("Expected an integer within the int range.");
	}
	else if (type == "float") JsonDetail::ReadFloatArray<1>(json::array({ value }));
	else if (type == "vector3" || type == "color") JsonDetail::ReadFloatArray<3>(value);
	else if (type == "string") { if (!value.is_string()) throw std::invalid_argument("Expected a string."); }
	else if (type == "bool") { if (!value.is_boolean()) throw std::invalid_argument("Expected a boolean."); }
	else throw std::invalid_argument("Unknown exposed property type: " + type);
}
void RefreshProperties(const CustomBehaviour& script)
{
	script.properties.clear();
	// Preserve the user-facing non-const exposure hook. Its only responsibility is
	// registering pointers in mutable metadata, not changing authored values.
	const_cast<CustomBehaviour&>(script).ExposeVariables();
	std::unordered_set<std::string> names;
	for (const auto& property : script.properties)
	{
		const auto path = FieldPath(property.name);
		if (!names.insert(property.name).second) throw MetadataError(path, "Duplicate exposed property name.");
		if (!property.value) throw MetadataError(path, "Exposed property pointer is null.");
		TypeName(property.type);
	}
}
json ReadProperty(const ExposedProperty& property)
{
	switch (property.type)
	{
	case PropertyType::Int: return *static_cast<int*>(property.value);
	case PropertyType::Float: return *static_cast<float*>(property.value);
	case PropertyType::String: return *static_cast<std::string*>(property.value);
	case PropertyType::Bool: return *static_cast<bool*>(property.value);
	case PropertyType::Vector3:
	case PropertyType::Color:
	{
		const auto* v = static_cast<float*>(property.value);
		return json::array({ v[0], v[1], v[2] });
	}
	}
	throw std::invalid_argument("Unsupported exposed property type.");
}
void WriteProperty(const ExposedProperty& property, const json& value)
{
	switch (property.type)
	{
	case PropertyType::Int: *static_cast<int*>(property.value) = value.get<int>(); break;
	case PropertyType::Float: *static_cast<float*>(property.value) = value.get<float>(); break;
	case PropertyType::String: *static_cast<std::string*>(property.value) = value.get<std::string>(); break;
	case PropertyType::Bool: *static_cast<bool*>(property.value) = value.get<bool>(); break;
	case PropertyType::Vector3:
	case PropertyType::Color:
	{
		const auto v = JsonDetail::ReadFloatArray<3>(value);
		std::copy(v.begin(), v.end(), static_cast<float*>(property.value)); break;
	}
	}
}
}

void CustomBehaviour::ValidateSerializedData(const json& data, const SerializationContext&)
{
	JsonRead::Object(data);
	const auto name = JsonRead::Value<std::string>(data, "className", "");
	if (name.empty() || !ScriptRegistry::GetInstance().IsRegistered(name))
		throw MetadataError(".className", "Script class is not registered: " + name);
	JsonRead::Value(data, "enabled", true);
	if (!data.contains("fields")) return;
	if (!data.at("fields").is_object()) throw MetadataError(".fields", "Expected a fields object.");
	for (const auto& [name, field] : data.at("fields").items())
	{
		const auto path = FieldPath(name);
		try { JsonRead::Object(field); }
		catch (const std::exception& error) { throw MetadataError(path, error.what()); }
		std::string type;
		try { type = field.at("type").get<std::string>(); }
		catch (const std::exception& error) { throw MetadataError(path + ".type", error.what()); }
		if (!field.contains("value")) throw MetadataError(path + ".value", "Saved field is missing its value.");
		// Value/type compatibility belongs to the compiled exposure contract.
		// Obsolete fields are ignored even if their former type is no longer supported.
	}
}

void CustomBehaviour::SerializeData(json& out, const SerializationContext& context) const
{
	const auto name = ScriptRegistry::GetInstance().GetRegisteredName(*this);
	if (name.empty()) throw MetadataError(".className", "Script class is not registered.");
	RefreshProperties(*this);
	json fields = json::object();
	for (const auto& property : properties)
	{
		const auto type = TypeName(property.type);
		const auto value = ReadProperty(property);
		try { ValidateValue(type, value); }
		catch (const std::exception& error) { throw MetadataError(FieldPath(property.name) + ".value", error.what()); }
		fields[property.name] = { { "type", type }, { "value", value } };
	}
	out = { { "className", name }, { "enabled", isEnabled }, { "fields", std::move(fields) } };
	ValidateSerializedData(out, context);
}

void CustomBehaviour::DeserializeData(const json& data, LoadContext& context)
{
	ValidateSerializedData(data, { context.projectRoot, context.registry });
	RefreshProperties(*this);
	if (data.contains("fields"))
	{
		const auto& fields = data.at("fields");
		// Validate the entire compiled field contract before assigning any values.
		for (const auto& property : properties)
			if (fields.contains(property.name))
			{
				const auto& field = fields.at(property.name);
				if (field.at("type") != TypeName(property.type))
					throw MetadataError(FieldPath(property.name) + ".type", "Saved field type does not match the exposed property.");
				try { ValidateValue(TypeName(property.type), field.at("value")); }
				catch (const std::exception& error) { throw MetadataError(FieldPath(property.name) + ".value", error.what()); }
			}
		for (const auto& [name, field] : fields.items())
		{
			const auto it = std::find_if(properties.begin(), properties.end(), [&](const auto& property) { return property.name == name; });
			if (it != properties.end()) WriteProperty(*it, field.at("value"));
			else if (context.reportWarning) context.reportWarning(FieldPath(name), "Unknown saved script field; ignored.");
		}
	}
	// Loading restores authored state, without activation or enable notifications.
	isEnabled = JsonRead::Value(data, "enabled", isEnabled);
}
