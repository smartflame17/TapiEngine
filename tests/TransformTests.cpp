#include "../Scene/Transform.h"

#include <algorithm>
#include <iostream>
#include <limits>

namespace dx = DirectX;

namespace
{

float MatrixError(dx::FXMMATRIX expected, dx::CXMMATRIX actual)
{
	dx::XMFLOAT4X4 a, b;
	dx::XMStoreFloat4x4(&a, expected);
	dx::XMStoreFloat4x4(&b, actual);
	float error = 0.0f;
	for (int row = 0; row < 4; ++row)
	{
		for (int col = 0; col < 4; ++col)
		{
			if (!std::isfinite(b.m[row][col]))
				return (std::numeric_limits<float>::max)();
			error = (std::max)(error, std::abs(a.m[row][col] - b.m[row][col]));
		}
	}
	return error;
}

bool Check(const char* name, float error, float tolerance)
{
	const bool passed = error <= tolerance;
	std::cout << (passed ? "PASS " : "FAIL ") << name
		<< ": max matrix error = " << error << '\n';
	return passed;
}

Transform SampleTransform()
{
	Transform transform;
	transform.position = { 3.0f, -2.0f, 7.0f };
	transform.rotation = { dx::XMConvertToRadians(30.0f), dx::XMConvertToRadians(45.0f), dx::XMConvertToRadians(60.0f) };
	transform.scale = { 0.5f, 2.0f, 3.0f };
	return transform;
}

float RoundTripError()
{
	// Include combined rotations and both sides of each pitch singularity.
	const float pitches[] = { -179.0f, -91.0f, -90.001f, -90.0f, -89.999f, -45.0f, 0.0f, 30.0f, 89.999f, 90.0f, 90.001f, 91.0f, 179.0f };
	const float angles[] = { -170.0f, -90.0f, -35.0f, 0.0f, 45.0f, 90.0f, 160.0f };
	float error = 0.0f;
	for (float pitch : pitches)
	{
		for (float yaw : angles)
		{
			for (float roll : angles)
			{
				auto transform = SampleTransform();
				transform.rotation = { dx::XMConvertToRadians(pitch), dx::XMConvertToRadians(yaw), dx::XMConvertToRadians(roll) };
				const auto expected = MakeTransformMatrix(transform);
				const auto actual = MakeTransformMatrix(MakeTransformFromMatrix(expected));
				error = (std::max)(error, MatrixError(expected, actual));
			}
		}
	}
	return error;
}

float RepeatedRoundTripError()
{
	const auto expected = MakeTransformMatrix(SampleTransform());
	auto actual = expected;
	float error = 0.0f;
	for (int frame = 0; frame < 1000; ++frame)
	{
		actual = MakeTransformMatrix(MakeTransformFromMatrix(actual));
		error = (std::max)(error, MatrixError(expected, actual));
	}
	return error;
}

float DragError(bool local, bool parented)
{
	float error = 0.0f;
	for (int axis = 0; axis < 3; ++axis)
	{
		auto transform = SampleTransform();
		const auto scale = dx::XMMatrixScaling(transform.scale.x, transform.scale.y, transform.scale.z);
		const auto translation = dx::XMMatrixTranslation(transform.position.x, transform.position.y, transform.position.z);
		auto expectedRotation = dx::XMMatrixRotationRollPitchYaw(transform.rotation.x, transform.rotation.y, transform.rotation.z);
		const auto delta = dx::XMMatrixRotationAxis(dx::XMVectorSet(axis == 0 ? 1.0f : 0.0f, axis == 1 ? 1.0f : 0.0f, axis == 2 ? 1.0f : 0.0f, 0.0f), 0.01f);
		// A uniformly scaled, rotated parent preserves representable S/R/T matrices.
		const auto parent = parented
			? dx::XMMatrixScaling(2.0f, 2.0f, 2.0f) * dx::XMMatrixRotationRollPitchYaw(-0.3f, 0.7f, 0.2f) * dx::XMMatrixTranslation(-4.0f, 1.0f, 8.0f)
			: dx::XMMatrixIdentity();
		const auto inverseParent = dx::XMMatrixInverse(nullptr, parent);
		for (int frame = 0; frame < 1000; ++frame)
		{
			// Apply incremental rotations, as ImGuizmo does, then use the scene's
			// world-to-local conversion and feed the stored transform into the next frame.
			const auto rotation = dx::XMMatrixRotationRollPitchYaw(transform.rotation.x, transform.rotation.y, transform.rotation.z);
			const auto currentScale = dx::XMMatrixScaling(transform.scale.x, transform.scale.y, transform.scale.z);
			const auto currentTranslation = dx::XMMatrixTranslation(transform.position.x, transform.position.y, transform.position.z);
			expectedRotation = local ? delta * expectedRotation : expectedRotation * delta;
			const auto editedRotation = local ? delta * rotation : rotation * delta;
			const auto editedWorld = currentScale * editedRotation * currentTranslation * parent;
			transform = MakeTransformFromMatrix(editedWorld * inverseParent);
			const auto expectedWorld = scale * expectedRotation * translation * parent;
			error = (std::max)(error, MatrixError(expectedWorld, MakeTransformMatrix(transform) * parent));
		}
	}
	return error;
}

}

int main()
{
	const auto original = SampleTransform();
	const auto restored = MakeTransformFromMatrix(MakeTransformMatrix(original));
	std::cout << "(30, 45, 60) degrees round-trips to ("
		<< dx::XMConvertToDegrees(restored.rotation.x) << ", "
		<< dx::XMConvertToDegrees(restored.rotation.y) << ", "
		<< dx::XMConvertToDegrees(restored.rotation.z) << ")\n";
	bool passed = Check("637 S/R/T round trips including gimbal lock", RoundTripError(), 0.0001f);
	passed &= Check("1000 unchanged round trips", RepeatedRoundTripError(), 0.001f);
	passed &= Check("local incremental rotations on all axes", DragError(true, false), 0.002f);
	passed &= Check("world incremental rotations on all axes", DragError(false, false), 0.002f);
	passed &= Check("parented incremental rotations on all axes", DragError(true, true), 0.004f);
	return passed ? 0 : 1;
}
