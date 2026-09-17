#pragma once
#include "../Components/Rigidbody.h"
#include "../Components/Collider.h"
#include "../Scene/Scene.h"

class PhysicsComponentTestAccess
{
public:
	static b3BodyId Body(const Rigidbody& component) { return component.bodyId; }
	static b3BodyId Body(const Collider& component) { return component.standaloneBodyId; }
	static b3ShapeId Shape(const Collider& component) { return component.shapeId; }
	static void Select(Scene& scene, GameObject& object) { scene.selectedObject = &object; }
	static void Inspector(Rigidbody& body) { body.DrawInspectorContents(); }
	static void Inspector(Collider& collider) { collider.DrawInspectorContents(); }
};
