#include "Rigidbody.h"
#include "Collider.h"
#include "../Physics/Physics.h"
#include "../Physics/PhysicsMath.h"
#include "../Scene/GameObject.h"
#include "../imgui/imgui.h"
#include <box3d/box3d.h>

Rigidbody::Rigidbody() noexcept : Component(StaticType)
{
	const auto defaults = b3DefaultBodyDef();
	gravityScale = defaults.gravityScale;
	linearDamping = defaults.linearDamping;
	angularDamping = defaults.angularDamping;
	const auto& m = defaults.motionLocks;
	motionLocks = { m.linearX, m.linearY, m.linearZ, m.angularX, m.angularY, m.angularZ };
}

Rigidbody::~Rigidbody() { ReleaseBody(); }

void Rigidbody::EnsureBody(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT4& rotation, bool enabled) noexcept
{
	if (b3Body_IsValid(bodyId)) return;
	auto def = b3DefaultBodyDef();
	def.type = bodyType == Type::Dynamic ? b3_dynamicBody : b3_staticBody;
	def.position = PhysicsMath::Vector(position);
	def.rotation = PhysicsMath::Quaternion(rotation);
	def.isEnabled = enabled;
	bodyId = b3CreateBody(Physics::GetInstance().worldId, &def);
	b3Body_SetUserData(bodyId, TryGetGameObject());
	ApplySettings();
}

void Rigidbody::ReleaseBody() noexcept
{
	if (b3Body_IsValid(bodyId)) b3DestroyBody(bodyId);
	bodyId = b3_nullBodyId;
}

bool Rigidbody::SetType(Type value) noexcept
{
	if (value != Type::Static && value != Type::Dynamic) return false;
	if (bodyType == value) return true;
	bodyType = value;
	if (auto* owner = TryGetGameObject(); owner && !IsPendingInspectorRemoval()) owner->RecreatePhysicsBody();
	return true;
}

bool Rigidbody::SetGravityScale(float value) noexcept
{
	if (!PhysicsMath::IsFinite(value)) return false;
	gravityScale = value;
	ApplySettings();
	return true;
}

bool Rigidbody::SetLinearDamping(float value) noexcept
{
	if (!PhysicsMath::IsFinite(value) || value < 0) return false;
	linearDamping = value;
	ApplySettings();
	return true;
}

bool Rigidbody::SetAngularDamping(float value) noexcept
{
	if (!PhysicsMath::IsFinite(value) || value < 0) return false;
	angularDamping = value;
	ApplySettings();
	return true;
}

void Rigidbody::SetMotionLocks(const MotionLocks& value) noexcept { motionLocks = value; ApplySettings(); }

void Rigidbody::ApplySettings() noexcept
{
	if (!b3Body_IsValid(bodyId)) return;
	b3Body_SetGravityScale(bodyId, gravityScale);
	b3Body_SetLinearDamping(bodyId, linearDamping);
	b3Body_SetAngularDamping(bodyId, angularDamping);
	b3Body_SetMotionLocks(bodyId, { motionLocks.linearX, motionLocks.linearY, motionLocks.linearZ,
		motionLocks.angularX, motionLocks.angularY, motionLocks.angularZ });
	if (bodyType == Type::Dynamic && b3Body_IsEnabled(bodyId)) b3Body_SetAwake(bodyId, true);
}

bool Rigidbody::HasCollider() const noexcept
{
	return TryGetGameObject() && GetComponent<Collider>() != nullptr;
}

void Rigidbody::DrawInspectorContents() noexcept
{
	int type = static_cast<int>(bodyType);
	if (ImGui::Combo("Body type", &type, "Static\0Dynamic\0")) SetType(static_cast<Type>(type));
	float gravity = gravityScale, linear = linearDamping, angular = angularDamping;
	if (ImGui::DragFloat("Gravity scale", &gravity, 0.05f)) SetGravityScale(gravity);
	if (ImGui::DragFloat("Linear damping", &linear, 0.05f, 0, 100, "%.3f", ImGuiSliderFlags_AlwaysClamp)) SetLinearDamping(linear);
	if (ImGui::DragFloat("Angular damping", &angular, 0.05f, 0, 100, "%.3f", ImGuiSliderFlags_AlwaysClamp)) SetAngularDamping(angular);
	auto locks = motionLocks;
	bool changed = ImGui::Checkbox("Lock translation X", &locks.linearX);
	changed |= ImGui::Checkbox("Lock translation Y", &locks.linearY);
	changed |= ImGui::Checkbox("Lock translation Z", &locks.linearZ);
	changed |= ImGui::Checkbox("Lock rotation X", &locks.angularX);
	changed |= ImGui::Checkbox("Lock rotation Y", &locks.angularY);
	changed |= ImGui::Checkbox("Lock rotation Z", &locks.angularZ);
	if (changed) SetMotionLocks(locks);
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0.75f, 0.25f, 1));
	if (!HasCollider()) ImGui::TextWrapped("Warning: no active Collider. This body has no collision geometry or shape-derived mass.");
	if (suspended) ImGui::TextWrapped("%s", PhysicsMath::SuspendedWarning);
	ImGui::PopStyleColor();
}
