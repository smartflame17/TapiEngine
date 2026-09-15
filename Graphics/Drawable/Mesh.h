#pragma once
#include "DrawableBase.h"
#include "../PhongMaterial.h"
#include "../IBindable/IBindableBase.h"
#include "../IBindable/IndexBuffer.h"
#include "../IBindable/ConstantBuffers.h"
#include "../IBindable/Sampler.h"
#include "../IBindable/Texture.h"
#include "../IBindable/Topology.h"
#include "../IBindable/TransformCBuf.h"
#include "../IBindable/SkinningCbuf.h"
#include <vector>
#include <memory>
#include <string>

class Mesh : public DrawableBase<Mesh>
{
public:
	Mesh(Graphics& gfx,
		std::vector<std::unique_ptr<IBindable>> bindPtrs,
		PhongMaterial material = {},
		PixelConstantBuffer<PhongMaterial>* pMaterialCbuf = nullptr,
		Texture* pBaseColorTexture = nullptr,
		Texture* pNormalTexture = nullptr,
		Sampler* pSampler = nullptr,
		std::string baseColorTexturePath = {},
		std::string normalMapPath = {},
		bool supportsTextureMapping = false,
		bool normalMapEnabled = false);
	void Draw(Graphics& gfx, DirectX::FXMMATRIX accumulatedTransform) const noexcept(!IS_DEBUG);
	void DrawShadow(Graphics& gfx, DirectX::FXMMATRIX accumulatedTransform, const ShadowDrawContext& context) const noexcept(!IS_DEBUG);
	void EnableSkinning(Graphics& gfx);
	void SetSkinningPalette(const std::vector<DirectX::XMFLOAT4X4>& palette);
	DirectX::XMMATRIX GetTransformXM() const noexcept override;
	void DrawInspector(Graphics& gfx, const char* label = nullptr) noexcept;
	void SetTransform(const Transform& transform) noexcept = delete;

private:
	SkinningCbuf* skinning = nullptr;
	void ApplyBaseColorTexturePath(Graphics& gfx) noexcept;
	void ApplyNormalMapPath(Graphics& gfx) noexcept;
	void RefreshMaterialState() noexcept;
	void UpdateBaseColorStatus(bool loadedFromFile) noexcept;
	void UpdateNormalMapStatus(bool loadedFromFile) noexcept;

	mutable DirectX::XMFLOAT4X4 transform = {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	};
	PhongMaterial material;
	PixelConstantBuffer<PhongMaterial>* pMaterialCbuf = nullptr;
	Texture* pBaseColorTexture = nullptr;
	Texture* pNormalTexture = nullptr;
	Sampler* pSampler = nullptr;
	std::string baseColorTexturePath;
	std::string baseColorTextureStatus;
	std::string normalMapPath;
	std::string normalMapStatus;
	bool supportsTextureMapping = false;
	bool normalMapEnabled = false;
};
