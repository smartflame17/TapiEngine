#pragma once
#include "Drawable.h"
#include "IndexedTriangleList.h"
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

	Primitive(Graphics& gfx, Shape shape, SurfaceMode surfaceMode, std::string texturePath = {});

	DirectX::XMMATRIX GetTransformXM() const noexcept override;
	void Draw(Graphics& gfx) const noexcept(!IS_DEBUG) override;
	void DrawInspector() noexcept override;

private:
	struct MaterialCbuf
	{
		DirectX::XMFLOAT3 color = { 0.8f, 0.8f, 0.8f };
		float specularIntensity = 0.5f;
		float specularPower = 32.0f;
		DirectX::XMFLOAT3 specularColor = { 1.0f, 1.0f, 1.0f };
	};

	static IndexedTriangleList BuildMesh(Shape shape, SurfaceMode surfaceMode);

	void ApplyTexturePath();
	void UpdateTextureStatus(bool loadedFromFile);
	const std::vector<std::unique_ptr<IBindable>>& GetStaticBinds() const noexcept override;

private:
	Graphics& gfx;
	Shape shape;
	SurfaceMode surfaceMode;
	MaterialCbuf material;
	std::string texturePath;
	std::string textureStatus;
	PixelConstantBuffer<MaterialCbuf>* pMaterialCbuf = nullptr;
	Texture* pTexture = nullptr;
	Sampler* pSampler = nullptr;
	std::vector<std::unique_ptr<IBindable>> staticBinds;
};
