#pragma once
#include "IndexedTriangleList.h"
#include "../Vertex.h"
#include <DirectXMath.h>
#include "../../Tools/TapiMath.h"
#include <optional>

class Sphere
{
public:
	// Same as plane, subdivide
	static IndexedTriangleList MakeTesselated(int latDiv, int longDiv, std::optional<Dvtx::VertexLayout> layout = {})
	{
		namespace dx = DirectX;
		using Type = Dvtx::VertexLayout::ElementType;
		assert(latDiv >= 3);
		assert(longDiv >= 3);

		if (!layout)
		{
			layout = Dvtx::VertexLayout{};
			layout->Append(Type::Position3D);
		}

		constexpr float radius = 1.0f;
		const auto base = dx::XMVectorSet(0.0f, 0.0f, radius, 0.0f);
		const float lattitudeAngle = PI / latDiv;
		const float longitudeAngle = 2.0f * PI / longDiv;

		Dvtx::VertexBuffer vertices{ std::move(*layout) };
		const auto& vertexLayout = vertices.GetLayout();
		const bool hasNormal = vertexLayout.Has(Type::Normal);
		const bool hasTexcoord = vertexLayout.Has(Type::Texture2D);

		for (int iLat = 1; iLat < latDiv; iLat++)
		{
			const float texV = float(iLat) / float(latDiv);
			const auto latBase = dx::XMVector3Transform(
				base,
				dx::XMMatrixRotationX(lattitudeAngle * iLat)
			);
			for (int iLong = 0; iLong < longDiv; iLong++)
			{
				vertices.Resize(vertices.Size() + 1u);
				auto vertex = vertices.Back();
				const auto positionVector = dx::XMVector3Transform(
					latBase,
					dx::XMMatrixRotationZ(longitudeAngle * iLong)
				);
				dx::XMStoreFloat3(&vertex.Attr<Type::Position3D>(), positionVector);
				if (hasNormal)
				{
					dx::XMStoreFloat3(&vertex.Attr<Type::Normal>(), dx::XMVector3Normalize(positionVector));
				}
				if (hasTexcoord)
				{
					vertex.Attr<Type::Texture2D>() = {
						float(iLong) / float(longDiv),
						1.0f - texV
					};
				}
			}
		}

		const auto iNorthPole = (unsigned short)vertices.Size();
		vertices.Resize(vertices.Size() + 1u);
		{
			auto vertex = vertices.Back();
			dx::XMStoreFloat3(&vertex.Attr<Type::Position3D>(), base);
			if (hasNormal)
			{
				vertex.Attr<Type::Normal>() = { 0.0f,0.0f,1.0f };
			}
			if (hasTexcoord)
			{
				vertex.Attr<Type::Texture2D>() = { 0.5f,0.0f };
			}
		}

		const auto iSouthPole = (unsigned short)vertices.Size();
		vertices.Resize(vertices.Size() + 1u);
		{
			auto vertex = vertices.Back();
			dx::XMStoreFloat3(&vertex.Attr<Type::Position3D>(), dx::XMVectorNegate(base));
			if (hasNormal)
			{
				vertex.Attr<Type::Normal>() = { 0.0f,0.0f,-1.0f };
			}
			if (hasTexcoord)
			{
				vertex.Attr<Type::Texture2D>() = { 0.5f,1.0f };
			}
		}

		const auto calcIdx = [longDiv](unsigned short iLat, unsigned short iLong)
			{
				return iLat * longDiv + iLong;
			};
		std::vector<unsigned short> indices;
		for (unsigned short iLat = 0; iLat < latDiv - 2; iLat++)
		{
			for (unsigned short iLong = 0; iLong < longDiv - 1; iLong++)
			{
				indices.push_back(calcIdx(iLat, iLong));
				indices.push_back(calcIdx(iLat + 1, iLong));
				indices.push_back(calcIdx(iLat, iLong + 1));
				indices.push_back(calcIdx(iLat, iLong + 1));
				indices.push_back(calcIdx(iLat + 1, iLong));
				indices.push_back(calcIdx(iLat + 1, iLong + 1));
			}
			// wrap band
			indices.push_back(calcIdx(iLat, longDiv - 1));
			indices.push_back(calcIdx(iLat + 1, longDiv - 1));
			indices.push_back(calcIdx(iLat, 0));
			indices.push_back(calcIdx(iLat, 0));
			indices.push_back(calcIdx(iLat + 1, longDiv - 1));
			indices.push_back(calcIdx(iLat + 1, 0));
		}

		// cap fans
		for (unsigned short iLong = 0; iLong < longDiv - 1; iLong++)
		{
			// north
			indices.push_back(iNorthPole);
			indices.push_back(calcIdx(0, iLong));
			indices.push_back(calcIdx(0, iLong + 1));
			// south
			indices.push_back(calcIdx(latDiv - 2, iLong + 1));
			indices.push_back(calcIdx(latDiv - 2, iLong));
			indices.push_back(iSouthPole);
		}
		// wrap triangles
		// north
		indices.push_back(iNorthPole);
		indices.push_back(calcIdx(0, longDiv - 1));
		indices.push_back(calcIdx(0, 0));
		// south
		indices.push_back(calcIdx(latDiv - 2, 0));
		indices.push_back(calcIdx(latDiv - 2, longDiv - 1));
		indices.push_back(iSouthPole);

		return { std::move(vertices),std::move(indices) };
	}

	// default sphere has 12x12 planes per hemisphere
	static IndexedTriangleList Make()
	{
		return MakeTesselated(12, 24);
	}
};
