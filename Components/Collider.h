#pragma once

#include "Component.h"
#include <DirectXMath.h>
#include <box3d/id.h>

class Collider final : public Component
{
public:
	static constexpr ComponentType StaticType = ComponentType::Collider;
	enum class Shape { Box, Sphere, Capsule };
	struct Geometry
	{
		Shape shape = Shape::Box;
		DirectX::XMFLOAT3 center = { 0, 0, 0 };
		DirectX::XMFLOAT3 size = { 1, 1, 1 }; // Full box dimensions.
		float radius = 0.5f;
		float height = 2.0f; // Capsule total height, including both caps; local Y axis.
	};

	Collider() noexcept;
	~Collider() override;
	Collider(const Collider&) = delete;
	Collider& operator=(const Collider&) = delete;
	const Geometry& GetGeometry() const noexcept { return geometry; }
	bool SetGeometry(const Geometry& value) noexcept;
	float GetDensity() const noexcept { return density; }
	bool SetDensity(float value) noexcept;
	float GetFriction() const noexcept { return friction; }
	bool SetFriction(float value) noexcept;
	float GetRestitution() const noexcept { return restitution; }
	bool SetRestitution(float value) noexcept;
	bool IsPhysicsSuspended() const noexcept { return suspended; }

private:
	friend class GameObject;
	friend class PhysicsComponentTestAccess;
	void EnsureStandaloneBody(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT4& rotation, bool enabled) noexcept;
	bool AttachShape(b3BodyId body, const DirectX::XMFLOAT3& scale) noexcept;
	void ReleaseShape() noexcept;
	void ReleaseStandaloneBody() noexcept;
	void ApplyMaterial() noexcept;
	void RefreshGeometry() noexcept;
	const char* GetInspectorTitle() const noexcept override { return "Collider"; }
	void DrawInspectorContents() noexcept override;

	b3ShapeId shapeId = b3_nullShapeId;
	b3BodyId standaloneBodyId = b3_nullBodyId;
	b3BodyId attachedBodyId = b3_nullBodyId;
	Geometry geometry;
	DirectX::XMFLOAT3 appliedScale = { 0, 0, 0 };
	float density = 1;
	float friction = 0;
	float restitution = 0;
	bool geometryDirty = true;
	bool suspended = false;
};
