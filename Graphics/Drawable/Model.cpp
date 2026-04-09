#include "Model.h"
#include "../IBindable/Sampler.h"
#include "../IBindable/Texture.h"
#include <assimp/material.h>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string_view>
#include <utility>

namespace
{
	PhongMaterial LoadMaterialConstants(const aiMaterial* pMaterial)
	{
		PhongMaterial material;
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

	std::filesystem::path ResolveNormalTexturePath(const aiMaterial* pMaterial, const std::filesystem::path& modelDirectory)
	{
		if (const auto path = ResolveTexturePath(pMaterial, aiTextureType_NORMALS, modelDirectory))
		{
			return *path;
		}
		if (const auto path = ResolveTexturePath(pMaterial, aiTextureType_HEIGHT, modelDirectory))
		{
			return *path;
		}
		return {};
	}

	void AppendBoundingBoxCorners(const DirectX::BoundingBox& box, DirectX::FXMMATRIX transform, std::vector<DirectX::XMFLOAT3>& points)
	{
		DirectX::XMFLOAT3 corners[DirectX::BoundingBox::CORNER_COUNT];
		box.GetCorners(corners);
		for (const auto& corner : corners)
		{
			DirectX::XMFLOAT3 transformed;
			DirectX::XMStoreFloat3(&transformed,
				DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&corner), transform));
			points.push_back(transformed);
		}
	}

	void CollectNodeBounds(const aiNode& node, DirectX::FXMMATRIX parentTransform, const std::vector<DirectX::BoundingBox>& meshBounds, std::vector<DirectX::XMFLOAT3>& points)
	{
		namespace dx = DirectX;
		const auto localTransform = dx::XMMatrixTranspose(dx::XMLoadFloat4x4(
			reinterpret_cast<const dx::XMFLOAT4X4*>(&node.mTransformation)
		));
		const auto worldTransform = localTransform * parentTransform;

		for (unsigned int meshIndex = 0; meshIndex < node.mNumMeshes; ++meshIndex)
		{
			const auto currentMeshIndex = node.mMeshes[meshIndex];
			AppendBoundingBoxCorners(meshBounds.at(currentMeshIndex), worldTransform, points);
		}

		for (unsigned int childIndex = 0; childIndex < node.mNumChildren; ++childIndex)
		{
			CollectNodeBounds(*node.mChildren[childIndex], worldTransform, meshBounds, points);
		}
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

void Node::DrawShadow(Graphics& gfx, DirectX::FXMMATRIX accumulatedTransform, ID3DBlob* pShadowVertexShaderBytecode) const noexcept(!IS_DEBUG)
{
	const auto localTransform =
		MakeTransformMatrix(relativeTransform) *
		DirectX::XMLoadFloat4x4(&bindLocalTransform);
	const auto builtTransform = localTransform * accumulatedTransform;
	for (const auto pMesh : meshPtrs)
	{
		pMesh->DrawShadow(gfx, builtTransform, pShadowVertexShaderBytecode);
	}
	for (const auto& pChild : childPtrs)
	{
		pChild->DrawShadow(gfx, builtTransform, pShadowVertexShaderBytecode);
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
	:
	gfx(gfx)
{
	Assimp::Importer importer;
	const auto pScene = importer.ReadFile(fileName.c_str(),
		aiProcess_Triangulate |
		aiProcess_JoinIdenticalVertices |
		aiProcess_GenSmoothNormals |
		aiProcess_ConvertToLeftHanded |
		aiProcess_CalcTangentSpace |
		aiProcess_GenBoundingBoxes
	);

	if (pScene == nullptr)
	{
		throw std::runtime_error("Failed to load model: " + fileName + "\n" + importer.GetErrorString());
	}

	const auto modelDirectory = std::filesystem::path(fileName).parent_path();
	for (size_t meshIndex = 0; meshIndex < pScene->mNumMeshes; meshIndex++)
	{
		DirectX::BoundingBox meshBounds;
		meshPtrs.push_back(ParseMesh(gfx, *pScene, *pScene->mMeshes[meshIndex], modelDirectory, meshBounds));
		this->meshBounds.push_back(meshBounds);
	}

	pRoot = ParseNode(*pScene->mRootNode);
	pSelectedNode = pRoot.get();

	std::vector<DirectX::XMFLOAT3> modelPoints;
	CollectNodeBounds(*pScene->mRootNode, DirectX::XMMatrixIdentity(), meshBounds, modelPoints);
	if (!modelPoints.empty())
	{
		DirectX::BoundingBox modelBounds;
		DirectX::BoundingBox::CreateFromPoints(modelBounds, modelPoints.size(), modelPoints.data(), sizeof(DirectX::XMFLOAT3));
		SetLocalBounds(modelBounds);
	}
}

void Model::Draw(Graphics& gfx) const noexcept(!IS_DEBUG)
{
	if (pRoot)
	{
		pRoot->Draw(gfx, GetAppliedTransformXM());
	}
}

void Model::DrawShadow(Graphics& gfx, ID3DBlob* pShadowVertexShaderBytecode) const noexcept(!IS_DEBUG)
{
	if (pRoot)
	{
		pRoot->DrawShadow(gfx, GetAppliedTransformXM(), pShadowVertexShaderBytecode);
	}
}

DirectX::XMMATRIX Model::GetTransformXM() const noexcept
{
	return GetAppliedTransformXM();
}

std::unique_ptr<Mesh> Model::ParseMesh(Graphics& gfx, const aiScene& scene, const aiMesh& mesh, const std::filesystem::path& modelDirectory, DirectX::BoundingBox& outBounds)
{
	namespace dx = DirectX;
	using Dvtx::VertexLayout;

	const bool hasTextureCoords = mesh.HasTextureCoords(0);
	auto layout = VertexLayout{}
		.Append(VertexLayout::Position3D)
		.Append(VertexLayout::Normal);
	if (hasTextureCoords)
	{
		layout.Append(VertexLayout::Texture2D)
			.Append(VertexLayout::Tangent);
	}
	Dvtx::VertexBuffer vertexBuffer(std::move(layout));
	std::vector<dx::XMFLOAT3> positions;
	positions.reserve(mesh.mNumVertices);

	for (unsigned int i = 0; i < mesh.mNumVertices; i++)
	{
		const auto position = *reinterpret_cast<const dx::XMFLOAT3*>(&mesh.mVertices[i]);
		positions.push_back(position);
		const auto normal = mesh.HasNormals() ?
			*reinterpret_cast<const dx::XMFLOAT3*>(&mesh.mNormals[i]) :
			dx::XMFLOAT3{ 0.0f, 1.0f, 0.0f };
		const auto tangent = mesh.HasTangentsAndBitangents() ?
			*reinterpret_cast<const dx::XMFLOAT3*>(&mesh.mTangents[i]) :
			dx::XMFLOAT3{ 1.0f, 0.0f, 0.0f };

		if (hasTextureCoords)
		{
			const dx::XMFLOAT2 texCoord = {
				mesh.mTextureCoords[0][i].x,
				mesh.mTextureCoords[0][i].y
			};
			vertexBuffer.EmplaceBack(position, normal, texCoord, tangent);
		}
		else
		{
			vertexBuffer.EmplaceBack(position, normal);
		}
	}

	DirectX::BoundingBox::CreateFromPoints(outBounds, positions.size(), positions.data(), sizeof(dx::XMFLOAT3));

	std::vector<std::uint32_t> indices;
	indices.reserve(mesh.mNumFaces * 3);
	for (unsigned int i = 0; i < mesh.mNumFaces; i++)
	{
		const auto& face = mesh.mFaces[i];
		assert(face.mNumIndices == 3);
		indices.push_back(face.mIndices[0]);
		indices.push_back(face.mIndices[1]);
		indices.push_back(face.mIndices[2]);
	}

	std::vector<std::unique_ptr<IBindable>> bindablePtrs;
	bindablePtrs.push_back(std::make_unique<Bind::VertexBuffer>(gfx, vertexBuffer));
	bindablePtrs.push_back(std::make_unique<IndexBuffer>(gfx, indices));

	const aiMaterial* pMaterial = mesh.mMaterialIndex < scene.mNumMaterials ? scene.mMaterials[mesh.mMaterialIndex] : nullptr;
	const auto material = LoadMaterialConstants(pMaterial);
	const auto baseColorTexturePath = hasTextureCoords ? ResolveBaseColorTexturePath(pMaterial, modelDirectory) : std::filesystem::path{};
	const auto normalTexturePath = hasTextureCoords ? ResolveNormalTexturePath(pMaterial, modelDirectory) : std::filesystem::path{};
	const bool useTexture = hasTextureCoords;

	auto pVertexShader = std::make_unique<VertexShader>(gfx, useTexture ? L"TexturedPhongVS.cso" : L"PhongVS.cso");
	auto pVertexShaderBytecode = pVertexShader->GetBytecode();
	bindablePtrs.push_back(std::move(pVertexShader));

	bindablePtrs.push_back(std::make_unique<PixelShader>(gfx, useTexture ? L"TexturedPhongPS.cso" : L"PhongPS.cso"));
	PixelConstantBuffer<PhongMaterial>* pMaterialCbuf = nullptr;
	Texture* pBaseColorTexture = nullptr;
	Texture* pNormalTexture = nullptr;
	Sampler* pSampler = nullptr;
	if (useTexture)
	{
		auto pBaseColorBindable = std::make_unique<Texture>(gfx, 0u);
		pBaseColorTexture = pBaseColorBindable.get();
		pBaseColorTexture->SetPath(gfx, baseColorTexturePath);
		bindablePtrs.push_back(std::move(pBaseColorBindable));

		auto pNormalBindable = std::make_unique<Texture>(gfx, 1u, Texture::FallbackKind::NeutralNormal);
		pNormalTexture = pNormalBindable.get();
		pNormalTexture->SetPath(gfx, normalTexturePath);
		bindablePtrs.push_back(std::move(pNormalBindable));

		auto pSamplerBindable = std::make_unique<Sampler>(gfx);
		pSampler = pSamplerBindable.get();
		bindablePtrs.push_back(std::move(pSamplerBindable));
	}
	bindablePtrs.push_back(std::make_unique<InputLayout>(gfx, vertexBuffer.GetLayout().GetD3DLayout(), pVertexShaderBytecode));
	auto pMaterialBindable = std::make_unique<PixelConstantBuffer<PhongMaterial>>(gfx, material, 0u);
	pMaterialCbuf = pMaterialBindable.get();
	bindablePtrs.push_back(std::move(pMaterialBindable));

	PhongMaterial meshMaterial = material;
	meshMaterial.useNormalMap = !normalTexturePath.empty() ? 1u : 0u;
	return std::make_unique<Mesh>(
		gfx,
		std::move(bindablePtrs),
		meshMaterial,
		pMaterialCbuf,
		pBaseColorTexture,
		pNormalTexture,
		pSampler,
		baseColorTexturePath.string(),
		normalTexturePath.string(),
		useTexture,
		!normalTexturePath.empty()
	);
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

	if (!pSelectedNode->meshPtrs.empty())
	{
		ImGui::Separator();
		ImGui::Text("Materials");
		for (size_t meshIndex = 0; meshIndex < pSelectedNode->meshPtrs.size(); ++meshIndex)
		{
			ImGui::PushID(static_cast<int>(meshIndex));
			std::string label = "Mesh " + std::to_string(meshIndex);
			pSelectedNode->meshPtrs[meshIndex]->DrawInspector(gfx, label.c_str());
			ImGui::PopID();
		}
	}
}
