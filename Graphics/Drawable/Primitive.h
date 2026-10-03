#pragma once
#include "Drawable.h"
#include "IndexedTriangleList.h"
#include "../PhongMaterial.h"
#include "../IBindable/ConstantBuffers.h"
#include "../IBindable/Sampler.h"
#include "../IBindable/Texture.h"
#include <string>

class Primitive : public Drawable
{
public:
	enum class Shape
	{
		Cone,
		Cube,
		Plane,
		Prism,
		Sphere
	};

	enum class SurfaceMode
	{
		Material,
		Textured
	};

	Primitive(Graphics& gfx, Shape shape, SurfaceMode surfaceMode, std::string texturePath = {}, std::string normalMapPath = {});
	Shape GetShape() const noexcept { return shape; }
	SurfaceMode GetSurfaceMode() const noexcept { return surfaceMode; }
	const PhongMaterial& GetMaterial() const noexcept { return material; }
	const std::string& GetTexturePath() const noexcept { return texturePath; }
	const std::string& GetNormalMapPath() const noexcept { return normalMapPath; }
	bool IsNormalMapEnabled() const noexcept { return normalMapEnabled; }
	Sampler::Type GetSamplerType() const noexcept;
	void SetSamplerType(Sampler::Type type);
	void SetMaterial(const PhongMaterial& value) noexcept;
	void SetResources(std::string textureReference, std::shared_ptr<const TextureAsset> texture,
		std::string normalReference, std::shared_ptr<const TextureAsset> normal, bool normalEnabled);

	DirectX::XMMATRIX GetTransformXM() const noexcept override;
	void Draw(Graphics& gfx) const noexcept(!IS_DEBUG) override;
	void DrawInspector() noexcept override;

private:
	static IndexedTriangleList BuildMesh(Shape shape, SurfaceMode surfaceMode);

	void ApplyTexturePath();
	void ApplyNormalMapPath();
	void RefreshMaterialState() noexcept;
	void UpdateTextureStatus(bool loadedFromFile);
	void UpdateNormalMapStatus(bool loadedFromFile);
	const std::vector<std::unique_ptr<IBindable>>& GetStaticBinds() const noexcept override;

private:
	Graphics& gfx;
	Shape shape;
	SurfaceMode surfaceMode;
	PhongMaterial material;
	std::string texturePath;
	std::string textureStatus;
	std::string normalMapPath;
	std::string normalMapStatus;
	bool normalMapEnabled = false;
	PixelConstantBuffer<PhongMaterial>* pMaterialCbuf = nullptr;
	Texture* pTexture = nullptr;
	Texture* pNormalTexture = nullptr;
	Sampler* pSampler = nullptr;
	std::vector<std::unique_ptr<IBindable>> staticBinds;
};
