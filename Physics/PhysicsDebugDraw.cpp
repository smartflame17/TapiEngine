#include "PhysicsDebugDraw.h"
#include <box3d/box3d.h>
#include <cmath>
#include <memory>

namespace
{
constexpr int CircleSegments = 32;
constexpr float Pi = 3.14159265358979323846f;

void Line(std::vector<b3Vec3>& lines, b3Vec3 a, b3Vec3 b)
{
	lines.push_back(a);
	lines.push_back(b);
}

void Arc(std::vector<b3Vec3>& lines, b3Vec3 center, b3Vec3 u, b3Vec3 v,
	float radius, float start, float sweep, int segments)
{
	const auto point = [&](float angle) {
		return b3Add(center, b3MulSV(radius, b3Add(b3MulSV(std::cos(angle), u), b3MulSV(std::sin(angle), v))));
	};
	auto previous = point(start);
	for (int i = 1; i <= segments; ++i)
	{
		const auto next = point(start + sweep * static_cast<float>(i) / segments);
		Line(lines, previous, next);
		previous = next;
	}
}

void Sphere(std::vector<b3Vec3>& lines, const b3Sphere& sphere)
{
	Arc(lines, sphere.center, b3Vec3_axisX, b3Vec3_axisY, sphere.radius, 0, 2 * Pi, CircleSegments);
	Arc(lines, sphere.center, b3Vec3_axisY, b3Vec3_axisZ, sphere.radius, 0, 2 * Pi, CircleSegments);
	Arc(lines, sphere.center, b3Vec3_axisZ, b3Vec3_axisX, sphere.radius, 0, 2 * Pi, CircleSegments);
}

void Capsule(std::vector<b3Vec3>& lines, const b3Capsule& capsule)
{
	const auto axis = b3Normalize(b3Sub(capsule.center2, capsule.center1));
	const auto u = b3Perp(axis);
	const auto v = b3Cross(axis, u);
	for (const auto center : { capsule.center1, capsule.center2 })
		Arc(lines, center, u, v, capsule.radius, 0, 2 * Pi, CircleSegments);
	for (const auto radial : { u, v })
	{
		Arc(lines, capsule.center1, radial, axis, capsule.radius, Pi, Pi, CircleSegments / 2);
		Arc(lines, capsule.center2, radial, axis, capsule.radius, 0, Pi, CircleSegments / 2);
		const auto offset = b3MulSV(capsule.radius, radial);
		Line(lines, b3Add(capsule.center1, offset), b3Add(capsule.center2, offset));
		Line(lines, b3Sub(capsule.center1, offset), b3Sub(capsule.center2, offset));
	}
}

struct OverlapContext
{
	b3BodyId ownBody;
	bool overlapping = false;
};

bool Overlap(b3ShapeId other, void* context)
{
	auto& query = *static_cast<OverlapContext*>(context);
	if (!B3_ID_EQUALS(b3Shape_GetBody(other), query.ownBody)) query.overlapping = true;
	return !query.overlapping;
}
}

struct PhysicsDebugDraw::Shape
{
	b3ShapeId id;
	std::vector<b3Vec3> vertices;
	std::vector<b3Vec3> proxyPoints;
	float radius = 0;
};

struct PhysicsDebugDraw::DrawContext
{
	PhysicsDebugDraw& adapter;
	b3WorldId world;
	bool play;
};

void PhysicsDebugDraw::Attach(b3WorldDef& definition) noexcept
{
	definition.createDebugShape = CreateShape;
	definition.destroyDebugShape = DestroyShape;
	definition.userDebugShapeContext = this;
}

void* PhysicsDebugDraw::CreateShape(const b3DebugShape* source, void* context) noexcept
{
	auto& adapter = *static_cast<PhysicsDebugDraw*>(context);
	try
	{
		auto shape = std::make_unique<Shape>();
		shape->id = source->shapeId;
		switch (source->type)
		{
		case b3_hullShape:
		{
			const auto& hull = *source->hull;
			// Engine boxes have eight points. Other collider types are not supported yet.
			if (hull.vertexCount > B3_MAX_SHAPE_CAST_POINTS) return nullptr;
			const auto* points = b3GetHullPoints(&hull);
			const auto* edges = b3GetHullEdges(&hull);
			shape->proxyPoints.assign(points, points + hull.vertexCount);
			for (int i = 0; i < hull.edgeCount; ++i)
				if (i < edges[i].twin)
					Line(shape->vertices, points[edges[i].origin], points[edges[edges[i].twin].origin]);
			break;
		}
		case b3_sphereShape:
			shape->proxyPoints = { source->sphere->center };
			shape->radius = source->sphere->radius;
			Sphere(shape->vertices, *source->sphere);
			break;
		case b3_capsuleShape:
			shape->proxyPoints = { source->capsule->center1, source->capsule->center2 };
			shape->radius = source->capsule->radius;
			Capsule(shape->vertices, *source->capsule);
			break;
		default:
			return nullptr;
		}
		++adapter.shapeCount;
		return shape.release();
	}
	catch (...) { adapter.callbackError = std::current_exception(); return nullptr; }
}

void PhysicsDebugDraw::DestroyShape(void* shape, void* context) noexcept
{
	if (!shape) return;
	delete static_cast<Shape*>(shape);
	--static_cast<PhysicsDebugDraw*>(context)->shapeCount;
}

void PhysicsDebugDraw::DrawShape(void* userShape, b3WorldTransform transform, b3HexColor, void* context) noexcept
{
	auto& draw = *static_cast<DrawContext*>(context);
	if (draw.adapter.callbackError) return;
	try
	{
		const auto& shape = *static_cast<Shape*>(userShape);
		bool colliding;
		if (draw.play)
		{
			// Query one stored touching contact. No event buffers, wake-ups, or physics steps.
			b3ContactData contact;
			colliding = b3Shape_GetContactData(shape.id, &contact, 1) != 0;
		}
		else
		{
			// Rotate the point cloud about the body's position to match the query's origin.
			b3Vec3 points[B3_MAX_SHAPE_CAST_POINTS];
			for (std::size_t i = 0; i < shape.proxyPoints.size(); ++i)
				points[i] = b3RotateVector(transform.q, shape.proxyPoints[i]);
			const b3ShapeProxy proxy{ points, static_cast<int>(shape.proxyPoints.size()), shape.radius };
			OverlapContext overlap{ b3Shape_GetBody(shape.id) };
			b3World_OverlapShape(draw.world, transform.p, &proxy, b3DefaultQueryFilter(), Overlap, &overlap);
			colliding = overlap.overlapping;
		}
		auto& vertices = colliding ? draw.adapter.frame.collisionVertices : draw.adapter.frame.idleVertices;
		const auto world = b3ToRelativeTransform(transform, b3Pos_zero);
		for (const auto vertex : shape.vertices) vertices.push_back(b3TransformPoint(world, vertex));
	}
	catch (...) { draw.adapter.callbackError = std::current_exception(); }
}

const PhysicsDebugDrawFrame& PhysicsDebugDraw::Collect(b3WorldId world, bool isPlayMode, const b3AABB& bounds)
{
	Clear();
	if (isPlayMode && !settings.drawDuringPlay) return frame;
	auto draw = b3DefaultDebugDraw();
	draw.drawShapes = true;
	draw.drawingBounds = bounds;
	draw.DrawShapeFcn = DrawShape;
	DrawContext context{ *this, world, isPlayMode };
	draw.context = &context;
	b3World_Draw(world, &draw, B3_DEFAULT_MASK_BITS);
	if (callbackError)
	{
		const auto error = callbackError;
		Clear();
		std::rethrow_exception(error);
	}
	return frame;
}

void PhysicsDebugDraw::Clear() noexcept
{
	frame.idleVertices.clear();
	frame.collisionVertices.clear();
	callbackError = nullptr;
}
