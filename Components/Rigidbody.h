#pragma once

#include "Component.h"
#include <DirectXMath.h>
#include <box3d/id.h>

class Rigidbody final : public Component
{
public:
	static constexpr ComponentType StaticType = ComponentType::Rigidbody;
	enum class Type { Static, Dynamic };
	struct MotionLocks
	{
		bool linearX = false, linearY = false, linearZ = false;
		bool angularX = false, angularY = false, angularZ = false;
	};

	Rigidbody() noexcept;
	~Rigidbody() override;
	Rigidbody(const Rigidbody&) = delete;
	Rigidbody& operator=(const Rigidbody&) = delete;
	Type GetType() const noexcept { return bodyType; }
	bool SetType(Type type) noexcept;
	float GetGravityScale() const noexcept { return gravityScale; }
	bool SetGravityScale(float value) noexcept;
	float GetLinearDamping() const noexcept { return linearDamping; }
	bool SetLinearDamping(float value) noexcept;
	float GetAngularDamping() const noexcept { return angularDamping; }
	bool SetAngularDamping(float value) noexcept;
	const MotionLocks& GetMotionLocks() const noexcept { return motionLocks; }
	void SetMotionLocks(const MotionLocks& value) noexcept;
	bool IsPhysicsSuspended() const noexcept { return suspended; }
	bool HasCollider() const noexcept;

private:
	friend class GameObject;
	friend class PhysicsComponentTestAccess;
	void EnsureBody(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT4& rotation, bool enabled) noexcept;
	void ReleaseBody() noexcept;
	void ApplySettings() noexcept;
	const char* GetInspectorTitle() const noexcept override { return "Rigidbody"; }
	void DrawInspectorContents() noexcept override;

	b3BodyId bodyId = b3_nullBodyId;
	Type bodyType = Type::Static;
	float gravityScale = 1;
	float linearDamping = 0;
	float angularDamping = 0;
	MotionLocks motionLocks;
	bool suspended = false;
};
