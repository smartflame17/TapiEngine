#include "Physics.h"

void Physics::RigidBody::Integrate(float dt)
{
    if (invMass == 0.0f) return; // Static object

    previous = current; // Archive state for interpolation

    // Symplectic Euler Integration
    Vector3 acceleration = forceAccumulator * invMass;
    velocity += acceleration * dt;
    current.position += velocity * dt;

    // Simple damping
    velocity *= 0.99f;
    forceAccumulator = Vector3::Zero; // Clear forces
}
