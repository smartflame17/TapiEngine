#include "Mesh.h"
#include "../../imgui/imgui.h"

// Mesh
Mesh::Mesh(Graphics& gfx, std::vector<std::unique_ptr<IBindable>> bindPtrs)
{
	if (!IsStaticInitialized())
	{
		AddStaticBind(std::make_unique<Topology>(gfx, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST));
	}

	for (auto& pb : bindPtrs)
	{
		if (auto pi = dynamic_cast<IndexBuffer*>(pb.get()))
		{
			AddIndexBuffer(std::unique_ptr<IndexBuffer>{ pi });
			pb.release();
		}
		else
		{
			AddBind(std::move(pb));
		}
	}

	AddBind(std::make_unique<TransformCbuf>(gfx, *this));
}
void Mesh::Draw(Graphics& gfx, DirectX::FXMMATRIX accumulatedTransform) const noexcept(!IS_DEBUG)
{
	DirectX::XMStoreFloat4x4(&transform, accumulatedTransform);
	Drawable::Draw(gfx);
}
DirectX::XMMATRIX Mesh::GetTransformXM() const noexcept
{
	return DirectX::XMLoadFloat4x4(&transform);
}


// Node
Node::Node(std::vector<Mesh*> meshPtrs, const std::string& name, const DirectX::XMMATRIX& transform) noexcept(!IS_DEBUG)
	:
	meshPtrs(std::move(meshPtrs)),
	name(name)
{
	DirectX::XMStoreFloat4x4(&this->transform, transform);
}
void Node::Draw(Graphics& gfx, DirectX::FXMMATRIX accumulatedTransform) const noexcept(!IS_DEBUG)
{
	const auto localTransform =
		DirectX::XMMatrixScaling(appliedScale.x, appliedScale.y, appliedScale.z) *
		DirectX::XMMatrixRotationRollPitchYaw(appliedRotation.x, appliedRotation.y, appliedRotation.z) *
		DirectX::XMMatrixTranslation(appliedTranslation.x, appliedTranslation.y, appliedTranslation.z) *
		DirectX::XMLoadFloat4x4(&transform);
	const auto built = localTransform * accumulatedTransform;
	for (const auto pm : meshPtrs)
	{
		pm->Draw(gfx, built);
	}
	for (const auto& pc : childPtrs)
	{
		pc->Draw(gfx, built);
	}
}
void Node::AddChild(std::unique_ptr<Node> pChild) noexcept(!IS_DEBUG)
{
	assert(pChild);
	childPtrs.push_back(std::move(pChild));
}

void Node::SetAppliedTransform(const DirectX::XMFLOAT3& translation, const DirectX::XMFLOAT3& rotation, const DirectX::XMFLOAT3& scale) noexcept
{
	appliedTranslation = translation;
	appliedRotation = rotation;
	appliedScale = scale;
}
const std::string& Node::GetName() const noexcept
{
	return name;
}
const std::vector<std::unique_ptr<Node>>& Node::GetChildren() const noexcept
{
	return childPtrs;
}
const DirectX::XMFLOAT3& Node::GetAppliedTranslation() const noexcept
{
	return appliedTranslation;
}
const DirectX::XMFLOAT3& Node::GetAppliedRotation() const noexcept
{
	return appliedRotation;
}
const DirectX::XMFLOAT3& Node::GetAppliedScale() const noexcept
{
	return appliedScale;
}

// Model
Model::Model(Graphics& gfx, const std::string fileName)
{
	Assimp::Importer imp;
	const auto pScene = imp.ReadFile(fileName.c_str(),
		aiProcess_Triangulate |
		aiProcess_JoinIdenticalVertices
	);

	for (size_t i = 0; i < pScene->mNumMeshes; i++)
	{
		meshPtrs.push_back(ParseMesh(gfx, *pScene->mMeshes[i]));
	}

	pRoot = ParseNode(*pScene->mRootNode);
	pSelectedNode = pRoot.get();
}
void Model::Draw(Graphics& gfx, DirectX::FXMMATRIX transform) const
{
	pRoot->Draw(gfx, transform);
}
std::unique_ptr<Mesh> Model::ParseMesh(Graphics& gfx, const aiMesh& mesh)
{
	namespace dx = DirectX;
	using Dvtx::VertexLayout;

	Dvtx::VertexBuffer vbuf(std::move(
		VertexLayout{}
		.Append(VertexLayout::Position3D)
		.Append(VertexLayout::Normal)
	));

	for (unsigned int i = 0; i < mesh.mNumVertices; i++)
	{
		vbuf.EmplaceBack(
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

	bindablePtrs.push_back(std::make_unique<VertexBuffer>(gfx, vbuf));

	bindablePtrs.push_back(std::make_unique<IndexBuffer>(gfx, indices));

	auto pvs = std::make_unique<VertexShader>(gfx, L"PhongVS.cso");
	auto pvsbc = pvs->GetBytecode();
	bindablePtrs.push_back(std::move(pvs));

	bindablePtrs.push_back(std::make_unique<PixelShader>(gfx, L"PhongPS.cso"));

	bindablePtrs.push_back(std::make_unique<InputLayout>(gfx, vbuf.GetLayout().GetD3DLayout(), pvsbc));

	struct PSMaterialConstant
	{
		DirectX::XMFLOAT3 color = { 0.8f,0.3f,0.8f };
		float padding;
	} pmc;
	bindablePtrs.push_back(std::make_unique<PixelConstantBuffer<PSMaterialConstant>>(gfx, pmc, 0u));

	return std::make_unique<Mesh>(gfx, std::move(bindablePtrs));
}
std::unique_ptr<Node> Model::ParseNode(const aiNode& node)
{
	namespace dx = DirectX;
	const auto transform = dx::XMMatrixTranspose(dx::XMLoadFloat4x4(
		reinterpret_cast<const dx::XMFLOAT4X4*>(&node.mTransformation)
	));

	std::vector<Mesh*> curMeshPtrs;
	curMeshPtrs.reserve(node.mNumMeshes);
	for (size_t i = 0; i < node.mNumMeshes; i++)
	{
		const auto meshIdx = node.mMeshes[i];
		curMeshPtrs.push_back(meshPtrs.at(meshIdx).get());
	}

	auto pNode = std::make_unique<Node>(std::move(curMeshPtrs), node.mName.C_Str(), transform);
	for (size_t i = 0; i < node.mNumChildren; i++)
	{
		pNode->AddChild(ParseNode(*node.mChildren[i]));
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
	if (!pRoot)
	{
		return;
	}

	if (ImGui::Begin("Inspector"))
	{
		ImGui::SetNextItemOpen(false, ImGuiCond_Once);
		DrawInspectorNode(*pRoot);
	}
	ImGui::End();

	if (!pSelectedNode)
	{
		pSelectedNode = pRoot.get();
	}

	auto translation = pSelectedNode->GetAppliedTranslation();
	auto rotation = pSelectedNode->GetAppliedRotation();
	auto scale = pSelectedNode->GetAppliedScale();

	if (ImGui::Begin("Transform"))
	{
		ImGui::Text("Node: %s", pSelectedNode->GetName().c_str());
		ImGui::Separator();

		ImGui::Text("Translate");
		ImGui::DragFloat3("##Translate", &translation.x, 0.05f);

		ImGui::Text("Rotate (radians)");
		ImGui::DragFloat3("##Rotate", &rotation.x, 0.01f);

		ImGui::Text("Scale");
		ImGui::DragFloat3("##Scale", &scale.x, 0.01f);

		if (ImGui::Button("Reset"))
		{
			translation = { 0.0f, 0.0f, 0.0f };
			rotation = { 0.0f, 0.0f, 0.0f };
			scale = { 1.0f, 1.0f, 1.0f };
		}
	}
	ImGui::End();

	pSelectedNode->SetAppliedTransform(translation, rotation, scale);
}
