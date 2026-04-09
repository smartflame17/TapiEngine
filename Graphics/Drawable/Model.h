#pragma once
#include "DrawableBase.h"
#include "Mesh.h"
#include "../IBindable/ConstantBuffers.h"
#include "../IBindable/InputLayout.h"
#include "../IBindable/PixelShader.h"
#include "../IBindable/VertexBuffer.h"
#include "../IBindable/VertexShader.h"
#include "../PhongMaterial.h"
#include "../Vertex.h"
#include "../../Scene/Transform.h"
#include "../../imgui/imgui.h"
#include "../../Components/Component.h"
#include <DirectXCollision.h>
#include <assimp/Importer.hpp>
#include <filesystem>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <string>
#include <stdexcept>
#include <cassert>
#include <vector>
#include <memory>

#pragma comment(lib, "assimp-vc143-mtd.lib")

class Node
{
	friend class Model;

public:
	Node(std::vector<Mesh*> meshPtrs, const std::string& name, const DirectX::XMMATRIX& transform) noexcept(!IS_DEBUG);
	void Draw(Graphics& gfx, DirectX::FXMMATRIX accumulatedTransform) const noexcept(!IS_DEBUG);
	void DrawShadow(Graphics& gfx, DirectX::FXMMATRIX accumulatedTransform, ID3DBlob* pShadowVertexShaderBytecode) const noexcept(!IS_DEBUG);
	void SetRelativeTransform(const Transform& transform) noexcept;
	const std::string& GetName() const noexcept;
	const std::vector<std::unique_ptr<Node>>& GetChildren() const noexcept;
	const Transform& GetRelativeTransform() const noexcept;

private:
	void AddChild(std::unique_ptr<Node> pChild) noexcept(!IS_DEBUG);

private:
	std::vector<std::unique_ptr<Node>> childPtrs;
	std::vector<Mesh*> meshPtrs;
	std::string name;
	DirectX::XMFLOAT4X4 bindLocalTransform;
	Transform relativeTransform;
};

class Model : public DrawableBase<Model>
{
public:
	Model(Graphics& gfx, const std::string& fileName);
	void Draw(Graphics& gfx) const noexcept(!IS_DEBUG) override;
	void DrawShadow(Graphics& gfx, ID3DBlob* pShadowVertexShaderBytecode) const noexcept(!IS_DEBUG) override;
	DirectX::XMMATRIX GetTransformXM() const noexcept override;

	void DrawInspector() noexcept override;
	void SpawnControlWindow() noexcept;

private:
	static std::unique_ptr<Mesh> ParseMesh(Graphics& gfx, const aiScene& scene, const aiMesh& mesh, const std::filesystem::path& modelDirectory, DirectX::BoundingBox& outBounds);
	std::unique_ptr<Node> ParseNode(const aiNode& node);
	void DrawInspectorNode(Node& node) noexcept;

private:
	Graphics& gfx;
	std::unique_ptr<Node> pRoot;
	Node* pSelectedNode = nullptr;
	std::vector<std::unique_ptr<Mesh>> meshPtrs;
	std::vector<DirectX::BoundingBox> meshBounds;
};
