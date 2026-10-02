#include "../Serialization/JsonMath.h"
#include "../Serialization/JsonEnums.h"
#include "../Serialization/SceneFormat.h"
#include "../Serialization/SerializationContext.h"
#include "../Serialization/SerializationError.h"
#include <iostream>
#include <limits>
#include <unordered_set>

using nlohmann::json;
namespace dx = DirectX;

namespace
{
int checks = 0;
void Check(bool condition, const char* message)
{
	++checks;
	if (!condition) throw std::runtime_error(message);
}
template<typename Fn> void ExpectRejected(Fn&& operation, const char* message)
{
	bool rejected = false;
	try { operation(); }
	catch (const std::exception&) { rejected = true; }
	Check(rejected, message);
}
template<typename T> T RoundTrip(const T& value)
{
	return json::parse(json(value).dump()).get<T>();
}
bool Near(float lhs, float rhs)
{
	return std::abs(static_cast<double>(lhs) - rhs) <= 1e-6 * (std::max)(1.0, std::abs(static_cast<double>(lhs)));
}
bool Equal(const dx::XMFLOAT3& lhs, const dx::XMFLOAT3& rhs)
{
	return Near(lhs.x, rhs.x) && Near(lhs.y, rhs.y) && Near(lhs.z, rhs.z);
}
bool Equal(const Transform& lhs, const Transform& rhs)
{
	return Equal(lhs.position, rhs.position) && Equal(lhs.rotation, rhs.rotation) && Equal(lhs.scale, rhs.scale);
}

void TestGuid()
{
	const std::string canonical = "219ef7b3-8b18-44de-9938-d7126ad24306";
	const auto known = Guid::FromString(canonical);
	Check(known.ToString() == canonical, "UUID text is preserved in canonical byte order");
	Check(known.GetBytes()[0] == 0x21 && known.GetBytes()[3] == 0xb3 && known.GetBytes()[15] == 0x06,
		"UUID bytes match the canonical text");
	Check(Guid::FromString("219EF7B3-8B18-44DE-9938-D7126AD24306") == known, "UUID input accepts uppercase hex");
	Check(RoundTrip(known) == known && json(known).is_string(), "UUID survives a JSON text round trip");
	Check(Guid{}.IsNull() && Guid::FromString(Guid{}.ToString()).IsNull(), "Null UUID is an explicit value");
	for (const auto* invalid : { "", "219ef7b38b1844de9938d7126ad24306", "{219ef7b3-8b18-44de-9938-d7126ad24306}",
		"219ef7b3_8b18-44de-9938-d7126ad24306", "219ef7b3-8b18-44de-9938-d7126ad2430g",
		"219ef7b3-8b18-44de-9938-d7126ad24306 ", "219ef7b3--b18-44de-9938-d7126ad24306" })
	{
		Check(!Guid::TryParse(invalid), "Malformed UUID is rejected by TryParse");
		ExpectRejected([&] { Guid::FromString(invalid); }, "Malformed UUID is rejected by FromString");
	}
	ExpectRejected([] { json(42).get<Guid>(); }, "UUID JSON must contain a string");
	std::unordered_set<Guid> generated;
	for (int i = 0; i < 1024; ++i)
	{
		const auto guid = Guid::Generate();
		Check(!guid.IsNull() && generated.insert(guid).second, "Generated UUID is non-null and unique");
		Check(Guid::FromString(guid.ToString()) == guid, "Generated UUID parses back to the same value");
	}
	Check(generated.contains(Guid::FromString(generated.begin()->ToString())), "UUID hash agrees with parsed equality");
}

void TestMath()
{
	const dx::XMFLOAT2 v2{ 0.1234567f, -9.876543f };
	const auto r2 = RoundTrip(v2);
	Check(Near(v2.x, r2.x) && Near(v2.y, r2.y) && json(v2).size() == 2, "XMFLOAT2 round trip");
	const dx::XMFLOAT3 v3{ -1.234567f, 2345.678f, 0.0000001f };
	Check(Equal(v3, RoundTrip(v3)) && json(v3).size() == 3, "XMFLOAT3 round trip");
	const dx::XMFLOAT4 v4{ 0.25f, 0.5f, 0.75f, 1.0f };
	const auto r4 = RoundTrip(v4);
	Check(Near(v4.x, r4.x) && Near(v4.y, r4.y) && Near(v4.z, r4.z) && Near(v4.w, r4.w)
		&& json(v4).size() == 4, "XMFLOAT4 round trip");
	const auto mixed = json::parse("[1, 2.5, -3]").get<dx::XMFLOAT3>();
	Check(Equal(mixed, { 1, 2.5f, -3 }), "Math arrays accept integer and floating point JSON numbers");
	const dx::XMFLOAT2 boundary{ (std::numeric_limits<float>::max)(), (std::numeric_limits<float>::lowest)() };
	const auto restoredBoundary = RoundTrip(boundary);
	Check(restoredBoundary.x == boundary.x && restoredBoundary.y == boundary.y, "Float range boundaries round trip");
	Transform transform;
	transform.position = v3;
	transform.rotation = { 0.1f, -1.3f, 3.14f };
	transform.scale = { -0.5f, 0.0f, 2.5f };
	Check(Equal(transform, RoundTrip(transform)), "Transform preserves position, Euler rotation and scale");
	Check(Equal(Transform{}, RoundTrip(Transform{})), "Default Transform round trip");
	auto document = json(transform);
	document["futureField"] = 42;
	Check(Equal(transform, document.get<Transform>()), "Unknown transform fields are ignored");
	for (const auto& invalid : { json(nullptr), json::object(), json::array({ 1, 2 }), json::array({ 1, 2, 3, 4 }),
		json::array({ 1, "2", 3 }), json::array({ 1, true, 3 }), json::array({ 1, nullptr, 3 }),
		json::array({ 1, 1e100, 3 }), json::array({ 1, std::numeric_limits<double>::infinity(), 3 }) })
		ExpectRejected([&] { invalid.get<dx::XMFLOAT3>(); }, "Malformed math array is rejected");
	ExpectRejected([] { json::array({ 1 }).get<dx::XMFLOAT2>(); }, "XMFLOAT2 requires exactly two values");
	ExpectRejected([] { json::array({ 1, 2, 3 }).get<dx::XMFLOAT4>(); }, "XMFLOAT4 requires exactly four values");
	for (const auto invalid : { std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(),
		std::numeric_limits<float>::quiet_NaN() })
	{
		ExpectRejected([&] { json out = dx::XMFLOAT2{ invalid, 0 }; }, "Non-finite XMFLOAT2 cannot be written");
		ExpectRejected([&] { json out = dx::XMFLOAT3{ 0, invalid, 0 }; }, "Non-finite XMFLOAT3 cannot be written");
		ExpectRejected([&] { json out = dx::XMFLOAT4{ 0, 0, 0, invalid }; }, "Non-finite XMFLOAT4 cannot be written");
	}
	dx::XMFLOAT3 unchanged = v3;
	ExpectRejected([&] { json::array({ 9, 8, "bad" }).get_to(unchanged); }, "Malformed vector assignment fails");
	Check(Equal(unchanged, v3), "Failed vector decode leaves the destination unchanged");
	document = json(transform);
	document["position"] = { 9, 8, 7 };
	document["scale"] = { 1, 2 };
	auto unchangedTransform = transform;
	ExpectRejected([&] { document.get_to(unchangedTransform); }, "Malformed transform assignment fails");
	Check(Equal(unchangedTransform, transform), "Failed transform decode leaves the destination unchanged");
	document = json(transform);
	document.erase("rotation");
	ExpectRejected([&] { document.get<Transform>(); }, "Required transform fields cannot be omitted");
	ExpectRejected([] { json::array().get<Transform>(); }, "Transform JSON must be an object");
}

template<typename Enum> void CheckEnum(Enum value, const char* text)
{
	Check(json(value) == text && RoundTrip(value) == value, "Enum uses its stable string and round trips");
	ExpectRejected([] { json("unknown").get<Enum>(); }, "Unknown enum strings fail");
	ExpectRejected([] { json(0).get<Enum>(); }, "Numeric enum ordinals fail");
	ExpectRejected([] { json out = static_cast<Enum>(200); }, "Invalid enum values cannot be written");
}
void TestEnums()
{
	CheckEnum(ComponentType::Drawable, "tapi.drawable"); CheckEnum(ComponentType::CustomBehaviour, "tapi.script");
	CheckEnum(ComponentType::SpotLight, "tapi.spot_light"); CheckEnum(ComponentType::PointLight, "tapi.point_light");
	CheckEnum(ComponentType::DirectionalLight, "tapi.directional_light"); CheckEnum(ComponentType::Camera, "tapi.camera");
	CheckEnum(ComponentType::Other, "tapi.other"); CheckEnum(ComponentType::Animator, "tapi.animator");
	CheckEnum(ComponentType::Rigidbody, "tapi.rigidbody"); CheckEnum(ComponentType::Collider, "tapi.collider");
	CheckEnum(ComponentType::AudioClip, "tapi.audio_clip");
	ExpectRejected([] { json out = ComponentType::Count; }, "ComponentType::Count is not a saved component type");
	CheckEnum(PropertyType::Int, "int"); CheckEnum(PropertyType::Float, "float"); CheckEnum(PropertyType::String, "string");
	CheckEnum(PropertyType::Vector3, "vector3"); CheckEnum(PropertyType::Color, "color"); CheckEnum(PropertyType::Bool, "bool");
	CheckEnum(Rigidbody::Type::Static, "static"); CheckEnum(Rigidbody::Type::Dynamic, "dynamic");
	CheckEnum(Collider::Shape::Box, "box"); CheckEnum(Collider::Shape::Sphere, "sphere"); CheckEnum(Collider::Shape::Capsule, "capsule");
	CheckEnum(Primitive::Shape::Cone, "cone"); CheckEnum(Primitive::Shape::Cube, "cube"); CheckEnum(Primitive::Shape::Plane, "plane");
	CheckEnum(Primitive::Shape::Prism, "prism"); CheckEnum(Primitive::Shape::Sphere, "sphere");
	CheckEnum(Primitive::SurfaceMode::Material, "material"); CheckEnum(Primitive::SurfaceMode::Textured, "textured");
	CheckEnum(LightType::None, "none"); CheckEnum(LightType::Directional, "directional");
	CheckEnum(LightType::Point, "point"); CheckEnum(LightType::Spot, "spot");
	CheckEnum(Animation::PlaybackState::Stopped, "stopped"); CheckEnum(Animation::PlaybackState::Playing, "playing");
	CheckEnum(Animation::PlaybackState::Paused, "paused"); CheckEnum(Animation::PlaybackState::Completed, "completed");
	auto value = Collider::Shape::Capsule;
	ExpectRejected([&] { json("unknown").get_to(value); }, "Unknown enum assignment fails");
	Check(value == Collider::Shape::Capsule, "Failed enum decode leaves the destination unchanged");
}
void TestResults()
{
	Check(SceneFormat::Name == "TapiScene" && SceneFormat::CurrentVersion == 1, "Scene format constants");
	LoadResult result;
	SerializationError diagnostic;
	diagnostic.jsonPath = "$.scene.objects[0].id";
	diagnostic.message = "Invalid UUID";
	result.warnings.push_back(diagnostic);
	Check(result.Succeeded() && static_cast<bool>(result), "Warnings allow a successful load result");
	result.errors.push_back(diagnostic);
	Check(!result.Succeeded() && !result, "Errors mark a load result as failed");
}
}

int main()
{
	try
	{
		TestGuid(); TestMath(); TestEnums(); TestResults();
		std::cout << "PASS " << checks << " serialization utility checks\n";
		return 0;
	}
	catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
