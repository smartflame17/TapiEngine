#include "DebugWireframeRenderer.h"
#include "Graphics.h"
#include "../ErrorHandling/GraphicsExceptionMacros.h"
#include <array>

using namespace DirectX;

namespace
{
	struct WireframeVertex
	{
		XMFLOAT3 position;
	};
}

DebugWireframeRenderer::DebugWireframeRenderer(Graphics& gfx)
{
	HRESULT hr;

	const std::array<WireframeVertex, 8> vertices =
	{ {
		{{-1.0f, -1.0f, -1.0f}},
		{{ 1.0f, -1.0f, -1.0f}},
		{{-1.0f,  1.0f, -1.0f}},
		{{ 1.0f,  1.0f, -1.0f}},
		{{-1.0f, -1.0f,  1.0f}},
		{{ 1.0f, -1.0f,  1.0f}},
		{{-1.0f,  1.0f,  1.0f}},
		{{ 1.0f,  1.0f,  1.0f}}
	} };

	const std::array<unsigned short, 24> indices =
	{ {
		0, 1, 1, 3, 3, 2, 2, 0,
		4, 5, 5, 7, 7, 6, 6, 4,
		0, 4, 1, 5, 2, 6, 3, 7
	} };

	D3D11_BUFFER_DESC vertexDesc = {};
	vertexDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertexDesc.Usage = D3D11_USAGE_DEFAULT;
	vertexDesc.ByteWidth = static_cast<UINT>(sizeof(WireframeVertex) * vertices.size());
	vertexDesc.StructureByteStride = sizeof(WireframeVertex);
	D3D11_SUBRESOURCE_DATA vertexData = {};
	vertexData.pSysMem = vertices.data();
	GFX_THROW_FAILED(gfx.pDevice->CreateBuffer(&vertexDesc, &vertexData, &pVertexBuffer));

	D3D11_BUFFER_DESC indexDesc = {};
	indexDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	indexDesc.Usage = D3D11_USAGE_DEFAULT;
	indexDesc.ByteWidth = static_cast<UINT>(sizeof(unsigned short) * indices.size());
	indexDesc.StructureByteStride = sizeof(unsigned short);
	D3D11_SUBRESOURCE_DATA indexData = {};
	indexData.pSysMem = indices.data();
	GFX_THROW_FAILED(gfx.pDevice->CreateBuffer(&indexDesc, &indexData, &pIndexBuffer));

	Microsoft::WRL::ComPtr<ID3DBlob> pVertexShaderBlob;
	GFX_THROW_FAILED(D3DReadFileToBlob(L"WireframeVS.cso", &pVertexShaderBlob));
	GFX_THROW_FAILED(gfx.pDevice->CreateVertexShader(
		pVertexShaderBlob->GetBufferPointer(),
		pVertexShaderBlob->GetBufferSize(),
		nullptr,
		&pVertexShader));

	Microsoft::WRL::ComPtr<ID3DBlob> pPixelShaderBlob;
	GFX_THROW_FAILED(D3DReadFileToBlob(L"WireframePS.cso", &pPixelShaderBlob));
	GFX_THROW_FAILED(gfx.pDevice->CreatePixelShader(
		pPixelShaderBlob->GetBufferPointer(),
		pPixelShaderBlob->GetBufferSize(),
		nullptr,
		&pPixelShader));

	const D3D11_INPUT_ELEMENT_DESC inputDesc[] =
	{
		{ "Position", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};
	GFX_THROW_FAILED(gfx.pDevice->CreateInputLayout(
		inputDesc,
		static_cast<UINT>(std::size(inputDesc)),
		pVertexShaderBlob->GetBufferPointer(),
		pVertexShaderBlob->GetBufferSize(),
		&pInputLayout));

	D3D11_BUFFER_DESC transformDesc = {};
	transformDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	transformDesc.Usage = D3D11_USAGE_DYNAMIC;
	transformDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	transformDesc.ByteWidth = sizeof(TransformCbuf);
	GFX_THROW_FAILED(gfx.pDevice->CreateBuffer(&transformDesc, nullptr, &pTransformBuffer));

	D3D11_BUFFER_DESC colorDesc = {};
	colorDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	colorDesc.Usage = D3D11_USAGE_DYNAMIC;
	colorDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	colorDesc.ByteWidth = sizeof(ColorCbuf);
	GFX_THROW_FAILED(gfx.pDevice->CreateBuffer(&colorDesc, nullptr, &pColorBuffer));

	D3D11_DEPTH_STENCIL_DESC depthDesc = {};
	depthDesc.DepthEnable = TRUE;
	depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	depthDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	GFX_THROW_FAILED(gfx.pDevice->CreateDepthStencilState(&depthDesc, &pDepthState));

	D3D11_RASTERIZER_DESC rasterDesc = {};
	rasterDesc.FillMode = D3D11_FILL_SOLID;
	rasterDesc.CullMode = D3D11_CULL_NONE;
	rasterDesc.DepthClipEnable = TRUE;
	rasterDesc.AntialiasedLineEnable = TRUE;
	GFX_THROW_FAILED(gfx.pDevice->CreateRasterizerState(&rasterDesc, &pRasterizerState));
}

void DebugWireframeRenderer::DrawBoundingBox(Graphics& gfx, const BoundingBox& bounds, const XMFLOAT3& color) noexcept(!IS_DEBUG)
{
	const UINT stride = sizeof(WireframeVertex);
	const UINT offset = 0u;
	gfx.pContext->IASetVertexBuffers(0u, 1u, pVertexBuffer.GetAddressOf(), &stride, &offset);
	gfx.pContext->IASetIndexBuffer(pIndexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0u);
	gfx.pContext->IASetInputLayout(pInputLayout.Get());
	gfx.pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
	gfx.pContext->VSSetShader(pVertexShader.Get(), nullptr, 0u);
	gfx.pContext->PSSetShader(pPixelShader.Get(), nullptr, 0u);
	gfx.pContext->OMSetDepthStencilState(pDepthState.Get(), 1u);
	gfx.pContext->RSSetState(pRasterizerState.Get());

	TransformCbuf transform = {};
	transform.modelViewProjection = XMMatrixTranspose(
		XMMatrixScaling(bounds.Extents.x, bounds.Extents.y, bounds.Extents.z) *
		XMMatrixTranslation(bounds.Center.x, bounds.Center.y, bounds.Center.z) *
		gfx.GetCamera() *
		gfx.GetProjection());

	D3D11_MAPPED_SUBRESOURCE mapped = {};
	gfx.pContext->Map(pTransformBuffer.Get(), 0u, D3D11_MAP_WRITE_DISCARD, 0u, &mapped);
	memcpy(mapped.pData, &transform, sizeof(transform));
	gfx.pContext->Unmap(pTransformBuffer.Get(), 0u);

	ColorCbuf colorData = {};
	colorData.color = color;
	gfx.pContext->Map(pColorBuffer.Get(), 0u, D3D11_MAP_WRITE_DISCARD, 0u, &mapped);
	memcpy(mapped.pData, &colorData, sizeof(colorData));
	gfx.pContext->Unmap(pColorBuffer.Get(), 0u);

	gfx.pContext->VSSetConstantBuffers(0u, 1u, pTransformBuffer.GetAddressOf());
	gfx.pContext->PSSetConstantBuffers(0u, 1u, pColorBuffer.GetAddressOf());
	gfx.DrawIndexed(24u);
}

void DebugWireframeRenderer::DrawBoundingBoxes(Graphics& gfx, const std::vector<BoundingBox>& bounds, const XMFLOAT3& color) noexcept(!IS_DEBUG)
{
	for (const auto& box : bounds)
	{
		DrawBoundingBox(gfx, box, color);
	}
	gfx.RestoreDefaultStates();
}
