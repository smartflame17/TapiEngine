#pragma once
#include "DrawableBase.h"
#include "Cube.h"
#include <filesystem>

class CubeMap : public DrawableBase<CubeMap>
{
public:
	CubeMap(Graphics& gfx, const std::filesystem::path& textureDirectory);
	DirectX::XMMATRIX GetTransformXM() const noexcept override;
};
