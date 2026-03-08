#include "Mesh.h"
#include "../IBindable/IBindableBase.h"
#include "../../ErrorHandling/GraphicsExceptionMacros.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#pragma comment(lib, "assimp-vc143-mtd.lib")

Mesh::Mesh(Graphics& gfx, const std::string& modelName)
{
	namespace dx = DirectX;

	struct Vertex
	{
		dx::XMFLOAT3 pos;
		dx::XMFLOAT3 n;
	};

	Assimp::Importer importer;
	const std::string modelPath = "Graphics/Models/" + modelName;
	const aiScene* scene = importer.ReadFile(
		modelPath,
		aiProcess_Triangulate |
		aiProcess_JoinIdenticalVertices |
		aiProcess_GenNormals |
		aiProcess_ConvertToLeftHanded
	);

	if (scene == nullptr || scene->mNumMeshes == 0u)
	{
		//throw SFWND_NOGFX_EXCEPT();
	}

	const aiMesh* pMesh = scene->mMeshes[0u];
	std::vector<Vertex> vertices;
	vertices.reserve(pMesh->mNumVertices);

	for (unsigned int i = 0u; i < pMesh->mNumVertices; i++)
	{
		const aiVector3D& v = pMesh->mVertices[i];
		const aiVector3D& n = pMesh->HasNormals() ? pMesh->mNormals[i] : aiVector3D(0.0f, 1.0f, 0.0f);
		vertices.push_back({ { v.x, v.y, v.z },{ n.x, n.y, n.z } });
	}

	std::vector<unsigned short> indices;
	indices.reserve(pMesh->mNumFaces * 3u);
	for (unsigned int i = 0u; i < pMesh->mNumFaces; i++)
	{
		const aiFace& face = pMesh->mFaces[i];
		if (face.mNumIndices == 3u)
		{
			indices.push_back(static_cast<unsigned short>(face.mIndices[0]));
			indices.push_back(static_cast<unsigned short>(face.mIndices[1]));
			indices.push_back(static_cast<unsigned short>(face.mIndices[2]));
		}
	}

	AddBind(std::make_unique<VertexBuffer>(gfx, vertices));
	AddIndexBuffer(std::make_unique<IndexBuffer>(gfx, indices));

	auto pvs = std::make_unique<VertexShader>(gfx, L"PhongVS.cso");
	auto pvsbc = pvs->GetBytecode();
	AddBind(std::move(pvs));
	AddBind(std::make_unique<PixelShader>(gfx, L"PhongPS.cso"));

	const std::vector<D3D11_INPUT_ELEMENT_DESC> ied =
	{
		{ "Position",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0 },
		{ "Normal",0,DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0 },
	};
	AddBind(std::make_unique<InputLayout>(gfx, ied, pvsbc));
	AddBind(std::make_unique<Topology>(gfx, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST));
	AddBind(std::make_unique<TransformCbuf>(gfx, *this));

	struct MaterialCbuf
	{
		dx::XMFLOAT3 color;
		float padding;
	};
	const MaterialCbuf material = { { 0.7f, 0.7f, 1.0f },0.0f };
	AddBind(std::make_unique<PixelConstantBuffer<MaterialCbuf>>(gfx, material, 0u));
}

DirectX::XMMATRIX Mesh::GetTransformXM() const noexcept
{
	return
		DirectX::XMMatrixScaling(scale, scale, scale) *
		DirectX::XMMatrixRotationRollPitchYaw(rot.x, rot.y, rot.z) *
		DirectX::XMMatrixTranslation(pos.x, pos.y, pos.z);
}

void Mesh::Update(float dt) noexcept
{
	(void)dt;
}

void Mesh::SetPos(DirectX::XMFLOAT3 newPos) noexcept
{
	pos = newPos;
}

void Mesh::SetRotation(float pitch, float yaw, float roll) noexcept
{
	rot = { pitch, yaw, roll };
}

void Mesh::SetScale(float newScale) noexcept
{
	scale = newScale;
}

const std::vector<std::unique_ptr<IBindable>>& Mesh::GetStaticBinds() const noexcept
{
	static const std::vector<std::unique_ptr<IBindable>> empty;
	return empty;
}
