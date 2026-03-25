#include "Primitive.h"
#include "Cone.h"
#include "Cube.h"
#include "Plane.h"
#include "Prism.h"
#include "Sphere.h"
#include "../IBindable/IBindableBase.h"
#include "../../imgui/imgui.h"
#include "../../Tools/TapiMath.h"
#include <array>
#include <cmath>
#include <filesystem>

namespace
{
	using Type = Dvtx::VertexLayout::ElementType;

	DirectX::BoundingBox ComputeBounds(const Dvtx::VertexBuffer& vertices)
	{
		std::vector<DirectX::XMFLOAT3> positions;
		positions.reserve(vertices.Size());
		for (std::size_t i = 0; i < vertices.Size(); ++i)
		{
			positions.push_back(vertices[i].Attr<Type::Position3D>());
		}

		DirectX::BoundingBox bounds;
		DirectX::BoundingBox::CreateFromPoints(bounds, positions.size(), positions.data(), sizeof(DirectX::XMFLOAT3));
		return bounds;
	}

	DirectX::XMFLOAT3 ComputeNormal(const DirectX::XMFLOAT3& p0, const DirectX::XMFLOAT3& p1, const DirectX::XMFLOAT3& p2)
	{
		namespace dx = DirectX;
		const auto edge1 = dx::XMVectorSubtract(dx::XMLoadFloat3(&p1), dx::XMLoadFloat3(&p0));
		const auto edge2 = dx::XMVectorSubtract(dx::XMLoadFloat3(&p2), dx::XMLoadFloat3(&p0));
		const auto normal = dx::XMVector3Normalize(dx::XMVector3Cross(
			edge1,
			edge2
		));

		dx::XMFLOAT3 result;
		dx::XMStoreFloat3(&result, normal);
		return result;
	}

	void AssignCubeAttributes(IndexedTriangleList& model, bool includeTexcoords)
	{
		static constexpr std::array<DirectX::XMFLOAT3, 6> faceNormals =
		{
			DirectX::XMFLOAT3{ 0.0f, 0.0f, -1.0f },
			DirectX::XMFLOAT3{ 0.0f, 0.0f, 1.0f },
			DirectX::XMFLOAT3{ -1.0f, 0.0f, 0.0f },
			DirectX::XMFLOAT3{ 1.0f, 0.0f, 0.0f },
			DirectX::XMFLOAT3{ 0.0f, -1.0f, 0.0f },
			DirectX::XMFLOAT3{ 0.0f, 1.0f, 0.0f }
		};
		static constexpr std::array<DirectX::XMFLOAT2, 4> faceTexcoords =
		{
			DirectX::XMFLOAT2{ 0.0f, 0.0f },
			DirectX::XMFLOAT2{ 1.0f, 0.0f },
			DirectX::XMFLOAT2{ 0.0f, 1.0f },
			DirectX::XMFLOAT2{ 1.0f, 1.0f }
		};

		for (size_t face = 0; face < faceNormals.size(); face++)
		{
			for (size_t vertexIndex = 0; vertexIndex < 4; vertexIndex++)
			{
				auto vertex = model.vertices[face * 4 + vertexIndex];
				vertex.Attr<Type::Normal>() = faceNormals[face];
				if (includeTexcoords)
				{
					vertex.Attr<Type::Texture2D>() = faceTexcoords[vertexIndex];
				}
			}
		}
	}

	void AppendVertex(
		Dvtx::VertexBuffer& vertices,
		const DirectX::XMFLOAT3& position,
		const DirectX::XMFLOAT3& normal,
		const DirectX::XMFLOAT2& texcoord)
	{
		const auto& layout = vertices.GetLayout();
		vertices.Resize(vertices.Size() + 1u);
		auto vertex = vertices.Back();
		vertex.Attr<Type::Position3D>() = position;
		if (layout.Has(Type::Normal))
		{
			vertex.Attr<Type::Normal>() = normal;
		}
		if (layout.Has(Type::Texture2D))
		{
			vertex.Attr<Type::Texture2D>() = texcoord;
		}
	}

	IndexedTriangleList MakePrismMesh(Dvtx::VertexLayout layout)
	{
		namespace dx = DirectX;
		constexpr float radius = 1.0f;
		constexpr float halfDepth = 1.0f;
		constexpr int longDiv = 24;
		const float longitudeAngle = 2.0f * PI / longDiv;

		Dvtx::VertexBuffer vertices(std::move(layout));
		std::vector<unsigned short> indices;
		indices.reserve(longDiv * 12u);

		const auto appendTriangle = [&](const DirectX::XMFLOAT3& p0,
			const DirectX::XMFLOAT3& p1,
			const DirectX::XMFLOAT3& p2,
			const DirectX::XMFLOAT3& normal,
			const DirectX::XMFLOAT2& uv0,
			const DirectX::XMFLOAT2& uv1,
			const DirectX::XMFLOAT2& uv2)
			{
				const auto baseIndex = static_cast<unsigned short>(vertices.Size());
				AppendVertex(vertices, p0, normal, uv0);
				AppendVertex(vertices, p1, normal, uv1);
				AppendVertex(vertices, p2, normal, uv2);
				indices.push_back(baseIndex);
				indices.push_back(baseIndex + 1u);
				indices.push_back(baseIndex + 2u);
			};

		for (int iLong = 0; iLong < longDiv; iLong++)
		{
			const float angle0 = longitudeAngle * iLong;
			const float angle1 = longitudeAngle * (iLong + 1);
			const DirectX::XMFLOAT3 near0 = { radius * std::cos(angle0), radius * std::sin(angle0), -halfDepth };
			const DirectX::XMFLOAT3 near1 = { radius * std::cos(angle1), radius * std::sin(angle1), -halfDepth };
			const DirectX::XMFLOAT3 far0 = { near0.x, near0.y, halfDepth };
			const DirectX::XMFLOAT3 far1 = { near1.x, near1.y, halfDepth };
			const DirectX::XMFLOAT3 sideNormal = ComputeNormal(near0, near1, far0);

			const float u0 = float(iLong) / float(longDiv);
			const float u1 = float(iLong + 1) / float(longDiv);

			appendTriangle(
				near0, near1, far0,
				sideNormal,
				{ u0, 1.0f }, { u1, 1.0f }, { u0, 0.0f }
			);
			appendTriangle(
				far0, near1, far1,
				sideNormal,
				{ u0, 0.0f }, { u1, 1.0f }, { u1, 0.0f }
			);

			const DirectX::XMFLOAT3 capNearCenter = { 0.0f, 0.0f, -halfDepth };
			const DirectX::XMFLOAT3 capFarCenter = { 0.0f, 0.0f, halfDepth };
			appendTriangle(
				capNearCenter, near1, near0,
				{ 0.0f, 0.0f, -1.0f },
				{ 0.5f, 0.5f },
				{ 0.5f + 0.5f * std::cos(angle1), 0.5f - 0.5f * std::sin(angle1) },
				{ 0.5f + 0.5f * std::cos(angle0), 0.5f - 0.5f * std::sin(angle0) }
			);
			appendTriangle(
				capFarCenter, far0, far1,
				{ 0.0f, 0.0f, 1.0f },
				{ 0.5f, 0.5f },
				{ 0.5f + 0.5f * std::cos(angle0), 0.5f - 0.5f * std::sin(angle0) },
				{ 0.5f + 0.5f * std::cos(angle1), 0.5f - 0.5f * std::sin(angle1) }
			);
		}

		return { std::move(vertices), std::move(indices) };
	}

	IndexedTriangleList MakeConeMesh(Dvtx::VertexLayout layout)
	{
		constexpr float radius = 1.0f;
		constexpr float baseZ = -1.0f;
		constexpr float tipZ = 1.0f;
		constexpr int longDiv = 24;
		const float longitudeAngle = 2.0f * PI / longDiv;

		Dvtx::VertexBuffer vertices(std::move(layout));
		std::vector<unsigned short> indices;
		indices.reserve(longDiv * 6u);

		const auto appendTriangle = [&](const DirectX::XMFLOAT3& p0,
			const DirectX::XMFLOAT3& p1,
			const DirectX::XMFLOAT3& p2,
			const DirectX::XMFLOAT3& normal,
			const DirectX::XMFLOAT2& uv0,
			const DirectX::XMFLOAT2& uv1,
			const DirectX::XMFLOAT2& uv2)
			{
				const auto baseIndex = static_cast<unsigned short>(vertices.Size());
				AppendVertex(vertices, p0, normal, uv0);
				AppendVertex(vertices, p1, normal, uv1);
				AppendVertex(vertices, p2, normal, uv2);
				indices.push_back(baseIndex);
				indices.push_back(baseIndex + 1u);
				indices.push_back(baseIndex + 2u);
			};

		const DirectX::XMFLOAT3 tip = { 0.0f, 0.0f, tipZ };
		const DirectX::XMFLOAT3 baseCenter = { 0.0f, 0.0f, baseZ };

		for (int iLong = 0; iLong < longDiv; iLong++)
		{
			const float angle0 = longitudeAngle * iLong;
			const float angle1 = longitudeAngle * (iLong + 1);
			const DirectX::XMFLOAT3 base0 = { radius * std::cos(angle0), radius * std::sin(angle0), baseZ };
			const DirectX::XMFLOAT3 base1 = { radius * std::cos(angle1), radius * std::sin(angle1), baseZ };
			const DirectX::XMFLOAT3 sideNormal = ComputeNormal(base0, base1, tip);

			const float u0 = float(iLong) / float(longDiv);
			const float u1 = float(iLong + 1) / float(longDiv);

			appendTriangle(
				base0, base1, tip,
				sideNormal,
				{ u0, 1.0f }, { u1, 1.0f }, { (u0 + u1) * 0.5f, 0.0f }
			);
			appendTriangle(
				baseCenter, base1, base0,
				{ 0.0f, 0.0f, -1.0f },
				{ 0.5f, 0.5f },
				{ 0.5f + 0.5f * std::cos(angle1), 0.5f - 0.5f * std::sin(angle1) },
				{ 0.5f + 0.5f * std::cos(angle0), 0.5f - 0.5f * std::sin(angle0) }
			);
		}

		return { std::move(vertices), std::move(indices) };
	}

	const char* ShapeLabel(Primitive::Shape shape) noexcept
	{
		switch (shape)
		{
		case Primitive::Shape::Cone:
			return "Cone";
		case Primitive::Shape::Cube:
			return "Cube";
		case Primitive::Shape::Plane:
			return "Plane";
		case Primitive::Shape::Prism:
			return "Prism";
		case Primitive::Shape::Sphere:
			return "Sphere";
		}
		return "Primitive";
	}
}

Primitive::Primitive(Graphics& gfx, Shape shape, SurfaceMode surfaceMode, std::string texturePath)
	:
	gfx(gfx),
	shape(shape),
	surfaceMode(surfaceMode),
	texturePath(std::move(texturePath))
{
	auto model = BuildMesh(shape, surfaceMode);
	SetLocalBounds(ComputeBounds(model.vertices));

	AddBind(std::make_unique<Bind::VertexBuffer>(gfx, model.vertices));
	AddIndexBuffer(std::make_unique<IndexBuffer>(gfx, model.indices));

	const bool isTextured = surfaceMode == SurfaceMode::Textured;
	auto pVertexShader = std::make_unique<VertexShader>(gfx, isTextured ? L"TexturedPhongVS.cso" : L"PhongVS.cso");
	const auto pVertexShaderBytecode = pVertexShader->GetBytecode();
	AddBind(std::move(pVertexShader));

	AddBind(std::make_unique<PixelShader>(gfx, isTextured ? L"TexturedPhongPS.cso" : L"PhongPS.cso"));
	AddBind(std::make_unique<InputLayout>(gfx, model.vertices.GetLayout().GetD3DLayout(), pVertexShaderBytecode));
	AddBind(std::make_unique<Topology>(gfx, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST));
	AddBind(std::make_unique<TransformCbuf>(gfx, *this));

	auto pMaterialBuffer = std::make_unique<PixelConstantBuffer<MaterialCbuf>>(gfx, material, 0u);
	pMaterialCbuf = pMaterialBuffer.get();
	AddBind(std::move(pMaterialBuffer));

	if (isTextured)
	{
		auto pTextureBindable = std::make_unique<Texture>(gfx, 0u);
		pTexture = pTextureBindable.get();
		AddBind(std::move(pTextureBindable));

		auto pSamplerBindable = std::make_unique<Sampler>(gfx, Sampler::Type::LinearWrap);
		pSampler = pSamplerBindable.get();
		AddBind(std::move(pSamplerBindable));

		ApplyTexturePath();
	}
}

DirectX::XMMATRIX Primitive::GetTransformXM() const noexcept
{
	return GetAppliedTransformXM();
}

void Primitive::Draw(Graphics& gfx) const noexcept(!IS_DEBUG)
{
	if (pMaterialCbuf != nullptr)
	{
		pMaterialCbuf->Update(gfx, material);
	}
	Drawable::Draw(gfx);
}

void Primitive::DrawInspector() noexcept
{
	ImGui::Text("Primitive");
	ImGui::Separator();
	ImGui::Text("Shape: %s", ShapeLabel(shape));
	ImGui::Text("Surface: %s", surfaceMode == SurfaceMode::Textured ? "Textured" : "Material");

	if (surfaceMode == SurfaceMode::Material)
	{
		ImGui::Separator();
		ImGui::ColorEdit3("Base Color", &material.color.x);
		ImGui::ColorEdit3("Specular Color", &material.specularColor.x);
		ImGui::SliderFloat("Specular Intensity", &material.specularIntensity, 0.0f, 2.0f, "%.2f");
		ImGui::SliderFloat("Specular Power", &material.specularPower, 1.0f, 128.0f, "%.1f");
		return;
	}

	static constexpr const char* samplerLabels[] =
	{
		"Linear Wrap",
		"Point Wrap",
		"Linear Clamp",
		"Point Clamp",
		"Anisotropic Wrap",
		"Anisotropic Clamp"
	};

	ImGui::Separator();
	int selectedSampler = static_cast<int>(pSampler != nullptr ? pSampler->GetType() : Sampler::Type::LinearWrap);
	if (ImGui::Combo("Sampler", &selectedSampler, samplerLabels, IM_ARRAYSIZE(samplerLabels)) && pSampler != nullptr)
	{
		pSampler->SetType(gfx, static_cast<Sampler::Type>(selectedSampler));
	}

	char texturePathBuffer[260] = {};
	texturePath.copy(texturePathBuffer, sizeof(texturePathBuffer) - 1u);
	if (ImGui::InputText("Texture Path", texturePathBuffer, IM_ARRAYSIZE(texturePathBuffer)))
	{
		texturePath = texturePathBuffer;
	}
	//ImGui::SameLine();
	if (ImGui::Button("Apply"))
	{
		ApplyTexturePath();
	}

	ImGui::TextWrapped("%s", textureStatus.c_str());
}

IndexedTriangleList Primitive::BuildMesh(Shape shape, SurfaceMode surfaceMode)
{
	auto layout = Dvtx::VertexLayout{}
		.Append(Type::Position3D)
		.Append(Type::Normal);
	if (surfaceMode == SurfaceMode::Textured)
	{
		layout.Append(Type::Texture2D);
	}

	switch (shape)
	{
	case Shape::Cone:
		return MakeConeMesh(std::move(layout));
	case Shape::Cube:
	{
		auto model = Cube::MakeIndependent(std::move(layout));
		AssignCubeAttributes(model, surfaceMode == SurfaceMode::Textured);
		return model;
	}
	case Shape::Plane:
	{
		auto model = Geometry::Plane::MakeTesselated(1, 1, std::move(layout));
		model.Transform(DirectX::XMMatrixTranslation(0.0f, -1.0f, 0.0f));
		return model;
	}
	case Shape::Prism:
		return MakePrismMesh(std::move(layout));
	case Shape::Sphere:
		return Sphere::MakeTesselated(12, 24, std::move(layout));
	}

	return Cube::MakeIndependent(std::move(layout));
}

void Primitive::ApplyTexturePath()
{
	if (pTexture == nullptr)
	{
		return;
	}

	const bool loadedFromFile = pTexture->SetPath(gfx, std::filesystem::path(texturePath));
	UpdateTextureStatus(loadedFromFile);
}

void Primitive::UpdateTextureStatus(bool loadedFromFile)
{
	if (loadedFromFile)
	{
		textureStatus = "Loaded texture: " + texturePath;
		return;
	}

	if (texturePath.empty())
	{
		textureStatus = "Using fallback checkerboard texture (empty path).";
		return;
	}

	textureStatus = "Using fallback checkerboard texture. Failed to load: " + texturePath;
}

const std::vector<std::unique_ptr<IBindable>>& Primitive::GetStaticBinds() const noexcept
{
	return staticBinds;
}
