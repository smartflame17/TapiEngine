#pragma once
#include "Drawable/IndexedTriangleList.h"
#include <array>
#include <cmath>

namespace TangentSpace
{
	inline DirectX::XMFLOAT3 BuildFallbackTangent(const DirectX::XMFLOAT3& normal) noexcept
	{
		namespace dx = DirectX;
		const dx::XMVECTOR n = dx::XMVector3Normalize(dx::XMLoadFloat3(&normal));
		const dx::XMVECTOR reference = std::abs(normal.y) < 0.999f
			? dx::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)
			: dx::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
		dx::XMFLOAT3 tangent;
		dx::XMStoreFloat3(&tangent, dx::XMVector3Normalize(dx::XMVector3Cross(reference, n)));
		return tangent;
	}

	inline void ComputeTangents(IndexedTriangleList& mesh) noexcept(!IS_DEBUG)
	{
		namespace dx = DirectX;
		using Type = Dvtx::VertexLayout::ElementType;

		const auto& layout = mesh.vertices.GetLayout();
		if (!layout.Has(Type::Position3D) || !layout.Has(Type::Normal) || !layout.Has(Type::Texture2D) || !layout.Has(Type::Tangent))
		{
			return;
		}

		std::vector<dx::XMFLOAT3> accumulatedTangents(mesh.vertices.Size(), { 0.0f, 0.0f, 0.0f });

		for (size_t index = 0; index + 2 < mesh.indices.size(); index += 3)
		{
			const auto i0 = mesh.indices[index];
			const auto i1 = mesh.indices[index + 1];
			const auto i2 = mesh.indices[index + 2];

			const auto p0 = mesh.vertices[i0].Attr<Type::Position3D>();
			const auto p1 = mesh.vertices[i1].Attr<Type::Position3D>();
			const auto p2 = mesh.vertices[i2].Attr<Type::Position3D>();
			const auto uv0 = mesh.vertices[i0].Attr<Type::Texture2D>();
			const auto uv1 = mesh.vertices[i1].Attr<Type::Texture2D>();
			const auto uv2 = mesh.vertices[i2].Attr<Type::Texture2D>();

			const dx::XMFLOAT3 edge1 = { p1.x - p0.x, p1.y - p0.y, p1.z - p0.z };
			const dx::XMFLOAT3 edge2 = { p2.x - p0.x, p2.y - p0.y, p2.z - p0.z };
			const dx::XMFLOAT2 deltaUV1 = { uv1.x - uv0.x, uv1.y - uv0.y };
			const dx::XMFLOAT2 deltaUV2 = { uv2.x - uv0.x, uv2.y - uv0.y };

			const float determinant = deltaUV1.x * deltaUV2.y - deltaUV1.y * deltaUV2.x;
			if (std::abs(determinant) < 1e-6f)
			{
				continue;
			}

			const float invDeterminant = 1.0f / determinant;
			const dx::XMFLOAT3 tangent =
			{
				invDeterminant * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x),
				invDeterminant * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y),
				invDeterminant * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z)
			};

			for (const auto vertexIndex : std::array<unsigned short, 3>{ i0, i1, i2 })
			{
				auto& accumulated = accumulatedTangents[vertexIndex];
				accumulated.x += tangent.x;
				accumulated.y += tangent.y;
				accumulated.z += tangent.z;
			}
		}

		for (size_t vertexIndex = 0; vertexIndex < mesh.vertices.Size(); ++vertexIndex)
		{
			auto vertex = mesh.vertices[vertexIndex];
			const auto normal = vertex.Attr<Type::Normal>();
			const dx::XMVECTOR normalVector = dx::XMVector3Normalize(dx::XMLoadFloat3(&normal));
			dx::XMVECTOR tangentVector = dx::XMLoadFloat3(&accumulatedTangents[vertexIndex]);

			if (dx::XMVectorGetX(dx::XMVector3LengthSq(tangentVector)) < 1e-6f)
			{
				vertex.Attr<Type::Tangent>() = BuildFallbackTangent(normal);
				continue;
			}

			tangentVector = dx::XMVector3Normalize(
				dx::XMVectorSubtract(
					tangentVector,
					dx::XMVectorScale(normalVector, dx::XMVectorGetX(dx::XMVector3Dot(tangentVector, normalVector)))
				)
			);
			if (dx::XMVectorGetX(dx::XMVector3LengthSq(tangentVector)) < 1e-6f)
			{
				vertex.Attr<Type::Tangent>() = BuildFallbackTangent(normal);
				continue;
			}

			dx::XMStoreFloat3(&vertex.Attr<Type::Tangent>(), tangentVector);
		}
	}
}
