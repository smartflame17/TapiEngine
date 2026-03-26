#pragma once

#include "../SmflmWin.h"
#include "SimpleMath.h"
#include <DirectXCollision.h>
#include <d3d11.h>
#include <wrl.h>
#include <vector>

class Graphics;

class DebugWireframeRenderer
{
public:
	explicit DebugWireframeRenderer(Graphics& gfx);

	void DrawBoundingBox(Graphics& gfx, const DirectX::BoundingBox& bounds, const DirectX::XMFLOAT3& color) noexcept(!IS_DEBUG);
	void DrawBoundingBoxes(Graphics& gfx, const std::vector<DirectX::BoundingBox>& bounds, const DirectX::XMFLOAT3& color) noexcept(!IS_DEBUG);

private:
	struct TransformCbuf
	{
		DirectX::XMMATRIX modelViewProjection;
	};

	struct ColorCbuf
	{
		DirectX::XMFLOAT3 color = { 0.0f, 1.0f, 0.0f };
		float padding = 0.0f;
	};

private:
	Microsoft::WRL::ComPtr<ID3D11Buffer> pVertexBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> pIndexBuffer;
	Microsoft::WRL::ComPtr<ID3D11VertexShader> pVertexShader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> pPixelShader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> pInputLayout;
	Microsoft::WRL::ComPtr<ID3D11Buffer> pTransformBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> pColorBuffer;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> pDepthState;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> pRasterizerState;
};
