#pragma once
#include <vector>
#include <memory>
#include <unordered_map>
#include "SimpleMath.h"

using namespace DirectX::SimpleMath;

namespace Physics
{
    // Snapshot of physics state for interpolation
    struct State
    {
        Vector3 position = Vector3::Zero;
        Quaternion rotation = Quaternion::Identity;
    };

    class RigidBody
    {
    public:
		RigidBody() = default;

        void SetPosition(const Vector3& pos) noexcept { current.position = pos; previous.position = pos; }
        void SetMass(float m) noexcept { mass = m; invMass = (m > 0.0f) ? 1.0f / m : 0.0f; }
        
		void AddForce(const Vector3& f) { forceAccumulator += f; }

        void Integrate(float dt);

        // Called during rendering (alpha = accumulator / dt)
        Matrix GetInterpolatedTransform(float alpha) const
        {
            Vector3 interpPos = Vector3::Lerp(previous.position, current.position, alpha);
            Quaternion interpRot = Quaternion::Slerp(previous.rotation, current.rotation, alpha);
            return Matrix::CreateFromQuaternion(interpRot) * Matrix::CreateTranslation(interpPos);
        }

        Vector3 GetPosition() const { return current.position; }

    public:
        Vector3 velocity = Vector3::Zero;
        float mass = 1.0f;

    private:
        State previous;
        State current;
        Vector3 forceAccumulator = Vector3::Zero;
        float invMass = 1.0f;
    };

	// For now, we use axis-aligned bounding boxes (AABB) for collision detection
    struct AABB
    {
        Vector3 min;
        Vector3 max;

        bool Intersects(const AABB& other) const
        {
            return (min.x <= other.max.x && max.x >= other.min.x) &&
                (min.y <= other.max.y && max.y >= other.min.y) &&
                (min.z <= other.max.z && max.z >= other.min.z);
        }
    };

    class Collider
    {
    public:
        Collider(RigidBody* body, Vector3 halfExtents)
            : attachedBody(body), halfExtents(halfExtents) {
        }

        AABB GetWorldAABB() const
        {
            Vector3 pos = attachedBody->GetPosition();
            return { pos - halfExtents, pos + halfExtents };
        }

        RigidBody* attachedBody = nullptr;
        Vector3 halfExtents; // For Box shape
        bool isColliding = false; // Debug flag
    };
}
