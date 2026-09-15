#include "Model.h"
#include "../IBindable/InputLayout.h"
#include "../IBindable/PixelShader.h"
#include "../IBindable/VertexBuffer.h"
#include "../IBindable/VertexShader.h"
#include "../ShadowDrawContext.h"
#include "../Vertex.h"
#include "../../imgui/imgui.h"
#include <cstring>
#include <stdexcept>

namespace dx = DirectX;

Model::Model(Graphics& gfx, const std::string& fileName) : Model(gfx, ImportModel(fileName)) {}

Model::Model(Graphics& gfx, std::shared_ptr<const ModelAsset> modelAsset)
    : gfx(gfx), asset(std::move(modelAsset))
{
    if (!asset) throw std::runtime_error("Model asset is null.");
    Animation::ValidateSkeleton(asset->skeleton);
    const auto& nodes = asset->skeleton.nodes;
    relativeTransforms.resize(nodes.size());
    children.resize(nodes.size());
    for (std::uint32_t n = 0; n < nodes.size(); ++n)
    {
        if (nodes[n].parent != Animation::NoNode) children[nodes[n].parent].push_back(n);
        for (auto m : nodes[n].meshes)
            instances.push_back({ n, m, CreateMesh(gfx, asset->meshes.at(m)) });
    }
    Animation::MakeBindPose(asset->skeleton, pose);
    UpdateGeometry();
}

void Model::Draw(Graphics& gfx) const noexcept(!IS_DEBUG)
{
    for (const auto& instance : instances)
        instance.mesh->Draw(gfx, dx::XMLoadFloat4x4(&pose.global[instance.nodeIndex]) * GetAppliedTransformXM());
}
void Model::DrawShadow(Graphics& gfx, const ShadowDrawContext& context) const noexcept(!IS_DEBUG)
{
    for (const auto& instance : instances)
        instance.mesh->DrawShadow(gfx, dx::XMLoadFloat4x4(&pose.global[instance.nodeIndex]) * GetAppliedTransformXM(), context);
}
dx::XMMATRIX Model::GetTransformXM() const noexcept { return GetAppliedTransformXM(); }

void Model::SetPose(const Animation::SkeletonPose& newPose)
{
    if (newPose.global.size() != asset->skeleton.nodes.size() || newPose.local.size() != newPose.global.size())
        throw std::runtime_error("Pose does not match model skeleton.");
    bindPose = false;
    if (std::memcmp(pose.global.data(), newPose.global.data(), pose.global.size() * sizeof(dx::XMFLOAT4X4)) == 0) return;
    pose = newPose;
    UpdateGeometry();
}
void Model::ResetPose()
{
    if (bindPose) return;
    Animation::MakeBindPose(asset->skeleton, pose);
    bindPose = true;
    UpdateGeometry();
}
void Model::RebuildStaticPose()
{
    for (std::size_t i = 0; i < pose.local.size(); ++i)
        dx::XMStoreFloat4x4(&pose.local[i], MakeTransformMatrix(relativeTransforms[i]) *
            dx::XMLoadFloat4x4(&asset->skeleton.nodes[i].bindLocal));
    Animation::AccumulatePose(asset->skeleton, pose);
    UpdateGeometry();
}
void Model::UpdateGeometry()
{
    dx::BoundingBox modelBounds = {};
    bool hasBounds = false;
    const auto include = [&](const dx::BoundingBox& box, dx::FXMMATRIX transform) {
        dx::BoundingBox transformed;
        box.Transform(transformed, transform);
        if (!hasBounds) { modelBounds = transformed; hasBounds = true; }
        else dx::BoundingBox::CreateMerged(modelBounds, modelBounds, transformed);
    };
    for (auto& instance : instances)
    {
        const auto& mesh = asset->meshes[instance.meshIndex];
        const auto meshGlobal = dx::XMLoadFloat4x4(&pose.global[instance.nodeIndex]);
        const auto& skin = mesh.skin;
        if (skin.boneNodes.empty()) { include(mesh.bounds, meshGlobal); continue; }
        const auto inverseMesh = dx::XMMatrixInverse(nullptr, meshGlobal);
        std::vector<dx::XMFLOAT4X4> palette(skin.boneNodes.size());
        for (std::size_t b = 0; b < palette.size(); ++b)
        {
            const auto meshToModel = dx::XMLoadFloat4x4(&skin.inverseBinds[b]) *
                dx::XMLoadFloat4x4(&pose.global[skin.boneNodes[b]]);
            dx::XMStoreFloat4x4(&palette[b], meshToModel * inverseMesh);
            if (skin.hasInfluenceBounds[b]) include(skin.influenceBounds[b], meshToModel);
        }
        if (skin.hasRigidBounds) include(skin.rigidBounds, meshGlobal);
        instance.mesh->SetSkinningPalette(palette);
    }
    if (hasBounds) SetLocalBounds(modelBounds);
}

std::unique_ptr<Mesh> Model::CreateMesh(Graphics& gfx, const MeshAsset& mesh)
{
    using Dvtx::VertexLayout;
    auto layout = VertexLayout{}.Append(VertexLayout::Position3D).Append(VertexLayout::Normal);
    if (mesh.hasUV) layout.Append(VertexLayout::Texture2D).Append(VertexLayout::Tangent);
    if (!mesh.skin.boneNodes.empty()) layout.Append(VertexLayout::BlendIndices).Append(VertexLayout::BlendWeights);
    Dvtx::VertexBuffer vertexBuffer(std::move(layout), mesh.vertices.size());
    for (std::size_t i = 0; i < mesh.vertices.size(); ++i)
    {
        const auto& source = mesh.vertices[i];
        auto vertex = vertexBuffer[i];
        vertex.Attr<VertexLayout::Position3D>() = source.position;
        vertex.Attr<VertexLayout::Normal>() = source.normal;
        if (mesh.hasUV)
        {
            vertex.Attr<VertexLayout::Texture2D>() = source.uv;
            vertex.Attr<VertexLayout::Tangent>() = source.tangent;
        }
        if (!mesh.skin.boneNodes.empty())
        {
            vertex.Attr<VertexLayout::BlendIndices>() = source.skin.indices;
            vertex.Attr<VertexLayout::BlendWeights>() = source.skin.weights;
        }
    }
	std::vector<std::unique_ptr<IBindable>> bindablePtrs;
	bindablePtrs.push_back(std::make_unique<Bind::VertexBuffer>(gfx, vertexBuffer));
	bindablePtrs.push_back(std::make_unique<IndexBuffer>(gfx, mesh.indices));

	const auto material = mesh.material;
	const auto& baseColorTexturePath = mesh.baseColorTexture;
	const auto& normalTexturePath = mesh.normalTexture;
	const bool useTexture = mesh.hasUV;
	const bool skinned = !mesh.skin.boneNodes.empty();
	auto pVertexShader = std::make_unique<VertexShader>(gfx, skinned ? (useTexture ? L"SkinnedTexturedPhongVS.cso" : L"SkinnedPhongVS.cso") : (useTexture ? L"TexturedPhongVS.cso" : L"PhongVS.cso"));
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
	auto result = std::make_unique<Mesh>(
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
	if (skinned) result->EnableSkinning(gfx);
	return result;
}

void Model::DrawInspectorNode(std::uint32_t index) noexcept
{
    const auto& node = asset->skeleton.nodes[index];
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;
    if (index == selectedNode) flags |= ImGuiTreeNodeFlags_Selected;
    if (children[index].empty()) flags |= ImGuiTreeNodeFlags_Leaf;
    const bool opened = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<uintptr_t>(index + 1)), flags, "%s", node.name.c_str());
    if (ImGui::IsItemClicked()) selectedNode = index;
    if (opened)
    {
        for (auto child : children[index]) DrawInspectorNode(child);
        ImGui::TreePop();
    }
}
void Model::SpawnControlWindow() noexcept
{
    if (ImGui::Begin("Model Control")) DrawInspector();
    ImGui::End();
}
void Model::DrawInspector() noexcept
{
    ImGui::Text("Model: %zu meshes%s", instances.size(), HasSkin() ? " (skinned)" : "");
    ImGui::SetNextItemOpen(false, ImGuiCond_Once);
    for (std::uint32_t i = 0; i < asset->skeleton.nodes.size(); ++i)
        if (asset->skeleton.nodes[i].parent == Animation::NoNode) DrawInspectorNode(i);
    ImGui::Separator();
    ImGui::Text("Node: %s", asset->skeleton.nodes[selectedNode].name.c_str());
    if (HasSkin()) ImGui::TextWrapped("Rig poses are controlled by Animator. Use the GameObject transform for placement.");
    ImGui::BeginDisabled(HasSkin());
    auto& relative = relativeTransforms[selectedNode];
    bool changed = ImGui::DragFloat3("Translate", &relative.position.x, 0.05f);
    changed |= ImGui::DragFloat3("Rotate (radians)", &relative.rotation.x, 0.01f);
    changed |= ImGui::DragFloat3("Scale", &relative.scale.x, 0.01f);
    if (ImGui::Button("Reset Node")) { relative = {}; changed = true; }
    ImGui::EndDisabled();
    if (changed) RebuildStaticPose();
    for (auto& instance : instances) if (instance.nodeIndex == selectedNode)
    {
        ImGui::PushID(instance.mesh.get());
        instance.mesh->DrawInspector(gfx, asset->meshes[instance.meshIndex].name.c_str());
        ImGui::PopID();
    }
}
