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
