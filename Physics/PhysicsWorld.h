#pragma once
#include "Physics.h"
#include "SpatialGrid.h"

namespace Physics
{
    class PhysicsWorld
    {
    public:
		PhysicsWorld() : spatialGrid(5.0f), gravity (0.0f) {} // 5.0 unit cell size / no gravity by default

        void AddBody(std::shared_ptr<RigidBody> rb, std::shared_ptr<Collider> col)
        {
            bodies.push_back(rb);
            colliders.push_back(col);
        }

		void SetGravity(float g) { gravity = g; }

		// Ran in fixed timesteps, so needs interpolation during rendering with rb->GetInterpolatedTransform(alpha)
        void Update(float dt)
        {
            // 1. Integrate RigidBodies
            for (auto& rb : bodies)
            {
                // Apply Gravity
                if (rb->mass > 0) rb->AddForce(Vector3(0, gravity * rb->mass, 0));
                rb->Integrate(dt);
            }

            // 2. Update Spatial Partition
            spatialGrid.Clear();
            for (auto& col : colliders)
            {
                spatialGrid.Insert(col.get());
                col->isColliding = false;
            }

            // 3. Collision Detection (Using Spatial Partition)
            for (auto& col : colliders)
            {
                auto candidates = spatialGrid.GetCandidates(col.get());
                AABB myBounds = col->GetWorldAABB();

                for (auto* other : candidates)
                {
                    if (myBounds.Intersects(other->GetWorldAABB()))
                    {
                        ResolveCollision(col.get(), other);
                    }
                }
            }
        }

    private:
        void ResolveCollision(Collider* a, Collider* b)
        {
            // Basic impulse resolution or logic
            a->isColliding = true;
            b->isColliding = true;

            // Simple bounce (velocity reflection)
            Vector3 relVel = b->attachedBody->velocity - a->attachedBody->velocity;
            // TODO: Full impulse resolution code would go here
        }

        std::vector<std::shared_ptr<RigidBody>> bodies;
        std::vector<std::shared_ptr<Collider>> colliders;
        SpatialGrid spatialGrid;
		float gravity;
    };
}