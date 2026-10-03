#pragma once
#include "DrawableBase.h"
#include "Mesh.h"
#include "../Assets/ModelAsset.h"
#include "../Assets/AssetLoader.h"
#include "../../Scene/Transform.h"

class Model : public DrawableBase<Model>
{
public:
    Model(Graphics& gfx, const std::string& fileName);
    Model(Graphics& gfx, std::shared_ptr<const ModelAsset> asset,
        std::filesystem::path sourcePath = {}, AssetLoader* assets = nullptr);
    const std::filesystem::path& GetSourcePath() const noexcept { return sourcePath; }
    const std::vector<Transform>& GetNodeTransforms() const noexcept { return relativeTransforms; }
    void SetNodeTransform(std::size_t index, const Transform& transform);
    std::size_t GetMeshInstanceCount() const noexcept { return instances.size(); }
    Mesh& GetMeshInstance(std::size_t index) { return *instances.at(index).mesh; }
    const Mesh& GetMeshInstance(std::size_t index) const { return *instances.at(index).mesh; }
    const MeshAsset& GetMeshSource(std::size_t index) const { return asset->meshes.at(instances.at(index).meshIndex); }
    void Draw(Graphics& gfx) const noexcept(!IS_DEBUG) override;
    void DrawShadow(Graphics& gfx, const ShadowDrawContext& context) const noexcept(!IS_DEBUG) override;
    DirectX::XMMATRIX GetTransformXM() const noexcept override;
    void DrawInspector() noexcept override;
    void SpawnControlWindow() noexcept;
    bool HasSkin() const noexcept { return asset->HasSkin(); }
    const std::shared_ptr<const ModelAsset>& GetAsset() const noexcept { return asset; }
    const Animation::SkeletonPose& GetPose() const noexcept { return pose; }
    void SetPose(const Animation::SkeletonPose& newPose);
    void ResetPose();
private:
    struct MeshInstance
    {
        std::uint32_t nodeIndex, meshIndex;
        std::unique_ptr<Mesh> mesh;
    };
    static std::unique_ptr<Mesh> CreateMesh(Graphics& gfx, const MeshAsset& mesh, AssetLoader* assets);
    void UpdateGeometry();
    void RebuildStaticPose();
    void DrawInspectorNode(std::uint32_t index) noexcept;
    Graphics& gfx;
    std::shared_ptr<const ModelAsset> asset;
    std::filesystem::path sourcePath;
    Animation::SkeletonPose pose;
    std::vector<Transform> relativeTransforms;
    std::vector<std::vector<std::uint32_t>> children;
    std::vector<MeshInstance> instances;
    std::uint32_t selectedNode = 0;
    bool bindPose = true;
};
