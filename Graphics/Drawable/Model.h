#pragma once

#include "Drawable.h"
#include "DrawableBase.h"
#include "../IBindable/IBindableBase.h"
#include "../Vertex.h"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <memory>
#include <string>
#include <vector>

#pragma comment(lib, "assimp-vc143-mtd.lib")

class Mesh : public DrawableBase<Mesh>
{
public:
	Mesh(Graphics& gfx, std::vector<std::unique_ptr<IBindable>> bindPtrs);
	void Draw(Graphics& gfx, DirectX::FXMMATRIX accumulatedTransform) const noexcept(!IS_DEBUG);
	DirectX::XMMATRIX GetTransformXM() const noexcept override;

private:
	mutable DirectX::XMFLOAT4X4 transform;
};

class Node
{
	friend class Model;

public:
	Node(std::vector<Mesh*> meshPtrs, const std::string& name, const DirectX::XMMATRIX& transform) noexcept(!IS_DEBUG);
	void Draw(Graphics& gfx, DirectX::FXMMATRIX accumulatedTransform) const noexcept(!IS_DEBUG);
	void SetAppliedTransform(const DirectX::XMFLOAT3& translation, const DirectX::XMFLOAT3& rotation, const DirectX::XMFLOAT3& scale) noexcept;
	const std::string& GetName() const noexcept;
	const std::vector<std::unique_ptr<Node>>& GetChildren() const noexcept;
	const DirectX::XMFLOAT3& GetAppliedTranslation() const noexcept;
	const DirectX::XMFLOAT3& GetAppliedRotation() const noexcept;
	const DirectX::XMFLOAT3& GetAppliedScale() const noexcept;

private:
	void AddChild(std::unique_ptr<Node> pChild) noexcept(!IS_DEBUG);

private:
	std::vector<std::unique_ptr<Node>> childPtrs;
	std::vector<Mesh*> meshPtrs;
	std::string name;
	DirectX::XMFLOAT4X4 transform;
	DirectX::XMFLOAT3 appliedTranslation = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 appliedRotation = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 appliedScale = { 1.0f, 1.0f, 1.0f };
};

class Model : public Drawable
{
public:
	Model(Graphics& gfx, const std::string& fileName);
	void Draw(Graphics& gfx) const noexcept(!IS_DEBUG) override;
	void Update(float dt) noexcept override;
	DirectX::XMMATRIX GetTransformXM() const noexcept override;

private:
	static std::unique_ptr<Mesh> ParseMesh(Graphics& gfx, const aiMesh& mesh);
	std::unique_ptr<Node> ParseNode(const aiNode& node);

private:
	std::unique_ptr<Node> pRoot;
	std::vector<std::unique_ptr<Mesh>> meshPtrs;
	DirectX::XMFLOAT3 modelTranslation = { 2.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 modelRotation = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 modelScale = { 5.0f, 5.0f, 5.0f };
};
