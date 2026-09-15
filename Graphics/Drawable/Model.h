#pragma once
#include "DrawableBase.h"
#include "Mesh.h"
#include "../Assets/ModelAsset.h"
#include "../../Scene/Transform.h"

class Model : public DrawableBase<Model>
{
public:
    Model(Graphics& gfx, const std::string& fileName);
    Model(Graphics& gfx, std::shared_ptr<const ModelAsset> asset);
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
    static std::unique_ptr<Mesh> CreateMesh(Graphics& gfx, const MeshAsset& mesh);
    void UpdateGeometry();
    void RebuildStaticPose();
    void DrawInspectorNode(std::uint32_t index) noexcept;
    Graphics& gfx;
    std::shared_ptr<const ModelAsset> asset;
    Animation::SkeletonPose pose;
    std::vector<Transform> relativeTransforms;
    std::vector<std::vector<std::uint32_t>> children;
    std::vector<MeshInstance> instances;
    std::uint32_t selectedNode = 0;
    bool bindPose = true;
};
