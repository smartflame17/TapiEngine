#pragma once
#include "Drawable.h"
#include <string>

class Mesh : public Drawable
{
public:
	Mesh(Graphics& gfx, const std::string& modelName);
	DirectX::XMMATRIX GetTransformXM() const noexcept override;
	void Update(float dt) noexcept override;

	void SetPos(DirectX::XMFLOAT3 pos) noexcept;
	void SetRotation(float pitch, float yaw, float roll) noexcept;
	void SetScale(float scale) noexcept;
private:
	const std::vector<std::unique_ptr<IBindable>>& GetStaticBinds() const noexcept override;
private:
	DirectX::XMFLOAT3 pos = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 rot = { 0.0f, 0.0f, 0.0f };
	float scale = 1.0f;
};
