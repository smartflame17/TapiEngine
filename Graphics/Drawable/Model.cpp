#include "Model.h"
#include "../IBindable/Sampler.h"
#include "../IBindable/Texture.h"
#include <assimp/material.h>
#include <filesystem>
#include <limits>
#include <optional>
#include <string_view>

namespace
{
	struct PSMaterialConstant
	{
		DirectX::XMFLOAT3 color = { 0.8f,0.3f,0.8f };
		float specularIntensity = 0.5f;
		float specularPower = 32.0f;
		DirectX::XMFLOAT3 specularColor = { 1.0f,1.0f,1.0f };
	};

	PSMaterialConstant LoadMaterialConstants(const aiMaterial* pMaterial)
	{
		PSMaterialConstant material;
		if (pMaterial == nullptr)
		{
			return material;
		}

		aiColor4D baseColor;
		if (pMaterial->Get(AI_MATKEY_BASE_COLOR, baseColor) == aiReturn_SUCCESS ||
			pMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, baseColor) == aiReturn_SUCCESS)
		{
			material.color = { baseColor.r, baseColor.g, baseColor.b };
		}

		aiColor4D specularColor;
		if (pMaterial->Get(AI_MATKEY_COLOR_SPECULAR, specularColor) == aiReturn_SUCCESS)
		{
			material.specularColor = { specularColor.r, specularColor.g, specularColor.b };
		}

		float shininess = 0.0f;
		if (pMaterial->Get(AI_MATKEY_SHININESS, shininess) == aiReturn_SUCCESS && shininess > 0.0f)
		{
			material.specularPower = shininess;
		}

		float specularStrength = 0.0f;
		if (pMaterial->Get(AI_MATKEY_SHININESS_STRENGTH, specularStrength) == aiReturn_SUCCESS)
		{
			material.specularIntensity = specularStrength;
		}

		return material;
	}

	std::optional<std::filesystem::path> ResolveTexturePath(const aiMaterial* pMaterial, aiTextureType textureType, const std::filesystem::path& modelDirectory)
	{
		if (pMaterial == nullptr || pMaterial->GetTextureCount(textureType) == 0)
		{
			return std::nullopt;
		}

		aiString texturePath;
		if (pMaterial->GetTexture(textureType, 0u, &texturePath) != aiReturn_SUCCESS)
		{
			return std::nullopt;
		}

		const std::string_view relativePath{ texturePath.C_Str() };
		if (relativePath.empty() || relativePath.front() == '*')
		{
			return std::nullopt;
		}

		auto resolvedPath = modelDirectory / std::filesystem::path(relativePath);
		if (!std::filesystem::exists(resolvedPath))
		{
			return std::nullopt;
		}

		return resolvedPath.lexically_normal();
	}

	std::filesystem::path ResolveBaseColorTexturePath(const aiMaterial* pMaterial, const std::filesystem::path& modelDirectory)
	{
		if (const auto path = ResolveTexturePath(pMaterial, aiTextureType_BASE_COLOR, modelDirectory))
		{
			return *path;
		}
		if (const auto path = ResolveTexturePath(pMaterial, aiTextureType_DIFFUSE, modelDirectory))
		{
			return *path;
		}
		return {};
	}
}

Node::Node(std::vector<Mesh*> meshPtrs, const std::string& name, const DirectX::XMMATRIX& transform) noexcept(!IS_DEBUG)
	:
	meshPtrs(std::move(meshPtrs)),
	name(name)
{
	DirectX::XMStoreFloat4x4(&this->bindLocalTransform, transform);
}

void Node::Draw(Graphics& gfx, DirectX::FXMMATRIX accumulatedTransform) const noexcept(!IS_DEBUG)
{
	const auto localTransform =
		MakeTransformMatrix(relativeTransform) *
		DirectX::XMLoadFloat4x4(&bindLocalTransform);
	const auto builtTransform = localTransform * accumulatedTransform;
	for (const auto pMesh : meshPtrs)
	{
		pMesh->Draw(gfx, builtTransform);
	}
	for (const auto& pChild : childPtrs)
	{
		pChild->Draw(gfx, builtTransform);
	}
}

void Node::AddChild(std::unique_ptr<Node> pChild) noexcept(!IS_DEBUG)
{
	assert(pChild);
	childPtrs.push_back(std::move(pChild));
}

void Node::SetRelativeTransform(const Transform& transform) noexcept
{
	relativeTransform = transform;
}

const std::string& Node::GetName() const noexcept
{
	return name;
}

const std::vector<std::unique_ptr<Node>>& Node::GetChildren() const noexcept
{
	return childPtrs;
}

const Transform& Node::GetRelativeTransform() const noexcept
{
	return relativeTransform;
}

Model::Model(Graphics& gfx, const std::string& fileName)
{
	Assimp::Importer importer;
	const auto pScene = importer.ReadFile(fileName.c_str(),
		aiProcess_Triangulate |
		aiProcess_JoinIdenticalVertices |
		aiProcess_GenSmoothNormals
	);

	if (pScene == nullptr)
	{
		throw std::runtime_error("Failed to load model: " + fileName + "\n" + importer.GetErrorString());
	}

	const auto modelDirectory = std::filesystem::path(fileName).parent_path();
	for (size_t meshIndex = 0; meshIndex < pScene->mNumMeshes; meshIndex++)
	{
		meshPtrs.push_back(ParseMesh(gfx, *pScene, *pScene->mMeshes[meshIndex], modelDirectory));
	}

	pRoot = ParseNode(*pScene->mRootNode);
	pSelectedNode = pRoot.get();
}

void Model::Draw(Graphics& gfx) const noexcept(!IS_DEBUG)
{
	if (pRoot)
	{
		pRoot->Draw(gfx, GetAppliedTransformXM());
	}
}

DirectX::XMMATRIX Model::GetTransformXM() const noexcept
{
	return GetAppliedTransformXM();
}

std::unique_ptr<Mesh> Model::ParseMesh(Graphics& gfx, const aiScene& scene, const aiMesh& mesh, const std::filesystem::path& modelDirectory)
{
	namespace dx = DirectX;
	using Dvtx::VertexLayout;

	if (mesh.mNumVertices > std::numeric_limits<unsigned short>::max())
	{
		throw std::runtime_error("Mesh has too many vertices for 16-bit indices: " + std::string(mesh.mName.C_Str()));
	}

	const bool hasTextureCoords = mesh.HasTextureCoords(0);
	auto layout = VertexLayout{}
		.Append(VertexLayout::Position3D)
		.Append(VertexLayout::Normal);
	if (hasTextureCoords)
	{
		layout.Append(VertexLayout::Texture2D);
	}
	Dvtx::VertexBuffer vertexBuffer(std::move(layout));

	for (unsigned int i = 0; i < mesh.mNumVertices; i++)
	{
		const auto position = *reinterpret_cast<const dx::XMFLOAT3*>(&mesh.mVertices[i]);
		const auto normal = mesh.HasNormals() ?
			*reinterpret_cast<const dx::XMFLOAT3*>(&mesh.mNormals[i]) :
			dx::XMFLOAT3{ 0.0f, 1.0f, 0.0f };

		if (hasTextureCoords)
		{
			const dx::XMFLOAT2 texCoord = {
				mesh.mTextureCoords[0][i].x,
				mesh.mTextureCoords[0][i].y
			};
			vertexBuffer.EmplaceBack(position, normal, texCoord);
		}
		else
		{
			vertexBuffer.EmplaceBack(position, normal);
		}
	}

	std::vector<unsigned short> indices;
	indices.reserve(mesh.mNumFaces * 3);
	for (unsigned int i = 0; i < mesh.mNumFaces; i++)
	{
		const auto& face = mesh.mFaces[i];
		assert(face.mNumIndices == 3);
		if (face.mIndices[0] > std::numeric_limits<unsigned short>::max() ||
			face.mIndices[1] > std::numeric_limits<unsigned short>::max() ||
			face.mIndices[2] > std::numeric_limits<unsigned short>::max())
		{
			throw std::runtime_error("Mesh index exceeds 16-bit range: " + std::string(mesh.mName.C_Str()));
		}
		indices.push_back(face.mIndices[0]);
		indices.push_back(face.mIndices[1]);
		indices.push_back(face.mIndices[2]);
	}

	std::vector<std::unique_ptr<IBindable>> bindablePtrs;
	bindablePtrs.push_back(std::make_unique<VertexBuffer>(gfx, vertexBuffer));
	bindablePtrs.push_back(std::make_unique<IndexBuffer>(gfx, indices));

	const aiMaterial* pMaterial = mesh.mMaterialIndex < scene.mNumMaterials ? scene.mMaterials[mesh.mMaterialIndex] : nullptr;
	const auto material = LoadMaterialConstants(pMaterial);
	const auto baseColorTexturePath = hasTextureCoords ? ResolveBaseColorTexturePath(pMaterial, modelDirectory) : std::filesystem::path{};
	const bool useTexture = hasTextureCoords && !baseColorTexturePath.empty();

	auto pVertexShader = std::make_unique<VertexShader>(gfx, useTexture ? L"TexturedPhongVS.cso" : L"PhongVS.cso");
	auto pVertexShaderBytecode = pVertexShader->GetBytecode();
	bindablePtrs.push_back(std::move(pVertexShader));

	bindablePtrs.push_back(std::make_unique<PixelShader>(gfx, useTexture ? L"TexturedPhongPS.cso" : L"PhongPS.cso"));
	if (useTexture)
	{
		bindablePtrs.push_back(std::make_unique<Texture>(gfx, baseColorTexturePath.wstring()));
		bindablePtrs.push_back(std::make_unique<Sampler>(gfx));
	}
	bindablePtrs.push_back(std::make_unique<InputLayout>(gfx, vertexBuffer.GetLayout().GetD3DLayout(), pVertexShaderBytecode));
	bindablePtrs.push_back(std::make_unique<PixelConstantBuffer<PSMaterialConstant>>(gfx, material, 0u));

	return std::make_unique<Mesh>(gfx, std::move(bindablePtrs));
}

std::unique_ptr<Node> Model::ParseNode(const aiNode& node)
{
	namespace dx = DirectX;
	const auto nodeTransform = dx::XMMatrixTranspose(dx::XMLoadFloat4x4(
		reinterpret_cast<const dx::XMFLOAT4X4*>(&node.mTransformation)
	));

	std::vector<Mesh*> currentMeshPtrs;
	currentMeshPtrs.reserve(node.mNumMeshes);
	for (size_t meshIndex = 0; meshIndex < node.mNumMeshes; meshIndex++)
	{
		const auto nodeMeshIndex = node.mMeshes[meshIndex];
		currentMeshPtrs.push_back(meshPtrs.at(nodeMeshIndex).get());
	}

	auto pNode = std::make_unique<Node>(std::move(currentMeshPtrs), node.mName.C_Str(), nodeTransform);
	for (size_t childIndex = 0; childIndex < node.mNumChildren; childIndex++)
	{
		pNode->AddChild(ParseNode(*node.mChildren[childIndex]));
	}

	return pNode;
}

void Model::DrawInspectorNode(Node& node) noexcept
{
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;
	if (&node == pSelectedNode)
	{
		flags |= ImGuiTreeNodeFlags_Selected;
	}
	if (node.GetChildren().empty())
	{
		flags |= ImGuiTreeNodeFlags_Leaf;
	}

	const bool isOpen = ImGui::TreeNodeEx(static_cast<void*>(&node), flags, "%s", node.GetName().c_str());
	if (ImGui::IsItemClicked())
	{
		pSelectedNode = &node;
	}

	if (isOpen)
	{
		for (const auto& pChild : node.GetChildren())
		{
			DrawInspectorNode(*pChild);
		}
		ImGui::TreePop();
	}
}

void Model::SpawnControlWindow() noexcept
{
	if (ImGui::Begin("Model Control"))
	{
		DrawInspector();
	}
	ImGui::End();
}

void Model::DrawInspector() noexcept
{
	if (!pRoot)
	{
		return;
	}
	ImGui::Text("Model");
	ImGui::Separator();
	ImGui::SetNextItemOpen(false, ImGuiCond_Once);
	DrawInspectorNode(*pRoot);

	if (!pSelectedNode)
	{
		pSelectedNode = pRoot.get();
	}

	auto relativeTransform = pSelectedNode->GetRelativeTransform();

	ImGui::Separator();
	ImGui::Text("Transform Controls");
	ImGui::NewLine();
	ImGui::Text("Node: %s", pSelectedNode->GetName().c_str());
	ImGui::Separator();

	ImGui::Text("Translate");
	ImGui::DragFloat3("##Translate", &relativeTransform.position.x, 0.05f);

	ImGui::Text("Rotate (radians)");
	ImGui::DragFloat3("##Rotate", &relativeTransform.rotation.x, 0.01f);

	ImGui::Text("Scale");
	ImGui::DragFloat3("##Scale", &relativeTransform.scale.x, 0.01f);

	if (ImGui::Button("Reset"))
	{
		relativeTransform = {};
	}

	pSelectedNode->SetRelativeTransform(relativeTransform);
}
