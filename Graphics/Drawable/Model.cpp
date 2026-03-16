#include "Model.h"

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

Model::Model(Graphics& gfx, const std::string& fileName, DirectX::XMMATRIX transform)
{
	DirectX::XMStoreFloat4x4(&modelTransform, transform);

	Assimp::Importer importer;
	const auto pScene = importer.ReadFile(fileName.c_str(),
		aiProcess_Triangulate |
		aiProcess_JoinIdenticalVertices
	);

	if (pScene == nullptr)
	{
		throw std::runtime_error("Failed to load model: " + fileName + "\n" + importer.GetErrorString());
	}

	for (size_t meshIndex = 0; meshIndex < pScene->mNumMeshes; meshIndex++)
	{
		meshPtrs.push_back(ParseMesh(gfx, *pScene->mMeshes[meshIndex]));
	}

	pRoot = ParseNode(*pScene->mRootNode);
	pSelectedNode = pRoot.get();
}

void Model::Draw(Graphics& gfx) const noexcept(!IS_DEBUG)
{
	if (pRoot)
	{
		const auto fullModelTransform = DirectX::XMLoadFloat4x4(&modelTransform) * GetAppliedTransformXM();
		pRoot->Draw(gfx, fullModelTransform);
	}
}

DirectX::XMMATRIX Model::GetTransformXM() const noexcept
{
	return DirectX::XMLoadFloat4x4(&modelTransform) * GetAppliedTransformXM();
}

std::unique_ptr<Mesh> Model::ParseMesh(Graphics& gfx, const aiMesh& mesh)
{
	namespace dx = DirectX;
	using Dvtx::VertexLayout;

	Dvtx::VertexBuffer vertexBuffer(std::move(
		VertexLayout{}
		.Append(VertexLayout::Position3D)
		.Append(VertexLayout::Normal)
	));

	for (unsigned int i = 0; i < mesh.mNumVertices; i++)
	{
		vertexBuffer.EmplaceBack(
			*reinterpret_cast<dx::XMFLOAT3*>(&mesh.mVertices[i]),
			*reinterpret_cast<dx::XMFLOAT3*>(&mesh.mNormals[i])
		);
	}

	std::vector<unsigned short> indices;
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
	bindablePtrs.push_back(std::make_unique<VertexBuffer>(gfx, vertexBuffer));
	bindablePtrs.push_back(std::make_unique<IndexBuffer>(gfx, indices));

	auto pVertexShader = std::make_unique<VertexShader>(gfx, L"PhongVS.cso");
	auto pVertexShaderBytecode = pVertexShader->GetBytecode();
	bindablePtrs.push_back(std::move(pVertexShader));

	bindablePtrs.push_back(std::make_unique<PixelShader>(gfx, L"PhongPS.cso"));
	bindablePtrs.push_back(std::make_unique<InputLayout>(gfx, vertexBuffer.GetLayout().GetD3DLayout(), pVertexShaderBytecode));

	struct PSMaterialConstant
	{
		DirectX::XMFLOAT3 color = { 0.8f,0.3f,0.8f };
		float specularIntensity = 0.7f;
		float specularPower = 32.0f;
		DirectX::XMFLOAT3 specularColor = { 1.0f,1.0f,1.0f };
	} material;
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
