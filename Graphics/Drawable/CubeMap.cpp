#include "CubeMap.h"
#include "../IBindable/CubeTexture.h"
#include "../IBindable/DepthStencilState.h"
#include "../IBindable/IBindableBase.h"
#include "../IBindable/RasterizerState.h"
#include "../IBindable/SkyboxTransformCbuf.h"

CubeMap::CubeMap(Graphics& gfx, const std::filesystem::path& textureDirectory)
{
	if (!IsStaticInitialized())
	{
		auto model = Cube::Make();

		AddStaticBind(std::make_unique<Bind::VertexBuffer>(gfx, model.vertices));
		AddStaticIndexBuffer(std::make_unique<IndexBuffer>(gfx, model.indices));

		auto pvs = std::make_unique<VertexShader>(gfx, L"Skybox_VS.cso");
		auto pvsbc = pvs->GetBytecode();
		AddStaticBind(std::move(pvs));
		AddStaticBind(std::make_unique<PixelShader>(gfx, L"Skybox_PS.cso"));

		AddStaticBind(std::make_unique<InputLayout>(gfx, model.vertices.GetLayout().GetD3DLayout(), pvsbc));
		AddStaticBind(std::make_unique<CubeTexture>(gfx, textureDirectory));
		AddStaticBind(std::make_unique<Sampler>(gfx));
		AddStaticBind(std::make_unique<DepthStencilState>(gfx, false, D3D11_DEPTH_WRITE_MASK_ZERO, D3D11_COMPARISON_ALWAYS));
		AddStaticBind(std::make_unique<RasterizerState>(gfx, D3D11_CULL_NONE));
		AddStaticBind(std::make_unique<Topology>(gfx, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST));
	}
	else
	{
		SetIndexFromStatic();
	}

	AddBind(std::make_unique<SkyboxTransformCbuf>(gfx));
}

DirectX::XMMATRIX CubeMap::GetTransformXM() const noexcept
{
	return DirectX::XMMatrixIdentity();
}
