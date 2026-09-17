#include "Collider.h"
#include "../Physics/Physics.h"
#include "../Physics/PhysicsMath.h"
#include "../Scene/GameObject.h"
#include "../imgui/imgui.h"
#include <box3d/box3d.h>
#include <algorithm>

Collider::Collider() noexcept : Component(StaticType)
{
	const auto defaults = b3DefaultShapeDef();
	density = defaults.density;
	friction = defaults.baseMaterial.friction;
	restitution = defaults.baseMaterial.restitution;
}

Collider::~Collider() { ReleaseShape(); ReleaseStandaloneBody(); }

void Collider::EnsureStandaloneBody(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT4& rotation, bool enabled) noexcept
{
	if (b3Body_IsValid(standaloneBodyId)) return;
	auto def = b3DefaultBodyDef();
	def.position = PhysicsMath::Vector(position);
	def.rotation = PhysicsMath::Quaternion(rotation);
	def.isEnabled = enabled;
	standaloneBodyId = b3CreateBody(Physics::GetInstance().worldId, &def);
	b3Body_SetUserData(standaloneBodyId, TryGetGameObject());
}

void Collider::ReleaseShape() noexcept
{
	if (b3Shape_IsValid(shapeId)) b3DestroyShape(shapeId, true);
	shapeId = b3_nullShapeId;
	attachedBodyId = b3_nullBodyId;
}

void Collider::ReleaseStandaloneBody() noexcept
{
	if (b3Body_IsValid(standaloneBodyId)) b3DestroyBody(standaloneBodyId);
	standaloneBodyId = b3_nullBodyId;
}

bool Collider::AttachShape(b3BodyId body, const DirectX::XMFLOAT3& scale) noexcept
{
	if (!geometryDirty && b3Shape_IsValid(shapeId) && B3_ID_EQUALS(body, attachedBodyId) &&
		scale.x == appliedScale.x && scale.y == appliedScale.y && scale.z == appliedScale.z) return true;
	const DirectX::XMFLOAT3 center{ geometry.center.x * scale.x, geometry.center.y * scale.y, geometry.center.z * scale.z };
	const DirectX::XMFLOAT3 size{ geometry.size.x * scale.x, geometry.size.y * scale.y, geometry.size.z * scale.z };
	const float radius = geometry.radius * (geometry.shape == Shape::Sphere ? (std::max)({ scale.x, scale.y, scale.z }) : (std::max)(scale.x, scale.z));
	const float height = (std::max)(geometry.height * scale.y, 2 * radius);
	if (!PhysicsMath::IsFinite(center) || !PhysicsMath::IsPositive(size) ||
		!PhysicsMath::IsFinite(radius) || radius <= 0 || !PhysicsMath::IsFinite(height) || height <= 0) return false;
	ReleaseShape();
	auto def = b3DefaultShapeDef();
	def.density = density;
	def.baseMaterial.friction = friction;
	def.baseMaterial.restitution = restitution;
	def.updateBodyMass = true;
	switch (geometry.shape)
	{
	case Shape::Box:
	{
		auto box = b3MakeTransformedBoxHull(size.x * 0.5f, size.y * 0.5f, size.z * 0.5f,
			{ PhysicsMath::Vector(center), b3Quat_identity });
		shapeId = b3CreateHullShape(body, &def, &box.base);
		break;
	}
	case Shape::Sphere:
	{
		const b3Sphere sphere{ PhysicsMath::Vector(center), radius };
		shapeId = b3CreateSphereShape(body, &def, &sphere);
		break;
	}
	case Shape::Capsule:
	{
		const float halfSegment = height * 0.5f - radius;
		// A capsule with no cylindrical segment is exactly a sphere.
		if (halfSegment <= 0)
		{
			const b3Sphere sphere{ PhysicsMath::Vector(center), radius };
			shapeId = b3CreateSphereShape(body, &def, &sphere);
		}
		else
		{
			const b3Capsule capsule{ { center.x, center.y - halfSegment, center.z },
				{ center.x, center.y + halfSegment, center.z }, radius };
			shapeId = b3CreateCapsuleShape(body, &def, &capsule);
		}
		break;
	}
	}
	if (!b3Shape_IsValid(shapeId)) return false;
	attachedBodyId = body;
	appliedScale = scale;
	geometryDirty = false;
	return true;
}

bool Collider::SetGeometry(const Geometry& value) noexcept
{
	if (value.shape != Shape::Box && value.shape != Shape::Sphere && value.shape != Shape::Capsule) return false;
	if (!PhysicsMath::IsFinite(value.center) || !PhysicsMath::IsPositive(value.size) ||
		!PhysicsMath::IsFinite(value.radius) || value.radius <= 0 || !PhysicsMath::IsFinite(value.height) || value.height <= 0) return false;
	geometry = value;
	RefreshGeometry();
	return true;
}

void Collider::RefreshGeometry() noexcept
{
	geometryDirty = true;
	if (auto* owner = TryGetGameObject(); owner && !IsPendingInspectorRemoval()) owner->RefreshPhysics();
}

bool Collider::SetDensity(float value) noexcept
{
	if (!PhysicsMath::IsFinite(value) || value <= 0) return false;
	density = value;
	ApplyMaterial();
	return true;
}

bool Collider::SetFriction(float value) noexcept
{
	if (!PhysicsMath::IsFinite(value) || value < 0) return false;
	friction = value;
	ApplyMaterial();
	return true;
}

bool Collider::SetRestitution(float value) noexcept
{
	if (!PhysicsMath::IsFinite(value) || value < 0 || value > 1) return false;
	restitution = value;
	ApplyMaterial();
	return true;
}

void Collider::ApplyMaterial() noexcept
{
	if (!b3Shape_IsValid(shapeId)) return;
	b3Shape_SetDensity(shapeId, density, true);
	b3Shape_SetFriction(shapeId, friction);
	b3Shape_SetRestitution(shapeId, restitution);
	if (b3Body_GetType(attachedBodyId) == b3_dynamicBody && b3Body_IsEnabled(attachedBodyId)) b3Body_SetAwake(attachedBodyId, true);
}

void Collider::DrawInspectorContents() noexcept
{
	auto edit = geometry;
	int shape = static_cast<int>(edit.shape);
	bool changed = ImGui::Combo("Shape", &shape, "Box\0Sphere\0Capsule\0");
	edit.shape = static_cast<Shape>(shape);
	changed |= ImGui::DragFloat3("Center", &edit.center.x, 0.05f);
	if (edit.shape == Shape::Box) changed |= ImGui::DragFloat3("Size", &edit.size.x, 0.05f, 0.001f, 10000, "%.3f", ImGuiSliderFlags_AlwaysClamp);
	else changed |= ImGui::DragFloat("Radius", &edit.radius, 0.05f, 0.001f, 10000, "%.3f", ImGuiSliderFlags_AlwaysClamp);
	if (edit.shape == Shape::Capsule) changed |= ImGui::DragFloat("Total height (Y)", &edit.height, 0.05f, 0.001f, 10000, "%.3f", ImGuiSliderFlags_AlwaysClamp);
	if (changed) SetGeometry(edit);
	float d = density, f = friction, r = restitution;
	if (ImGui::DragFloat("Density", &d, 0.05f, 0.001f, 10000, "%.3f", ImGuiSliderFlags_AlwaysClamp)) SetDensity(d);
	if (ImGui::DragFloat("Friction", &f, 0.01f, 0, 100, "%.3f", ImGuiSliderFlags_AlwaysClamp)) SetFriction(f);
	if (ImGui::DragFloat("Restitution", &r, 0.01f, 0, 1, "%.3f", ImGuiSliderFlags_AlwaysClamp)) SetRestitution(r);
	if (suspended)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0.75f, 0.25f, 1));
		ImGui::TextWrapped("%s", PhysicsMath::SuspendedWarning);
		ImGui::PopStyleColor();
	}
}
