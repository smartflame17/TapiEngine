#pragma once

#include "Graphics.h"
#include "RenderQueue.h"
#include "IBindable/BlendState.h"
#include "IBindable/ConstantBuffers.h"
#include "IBindable/DepthStencilState.h"
#include "IBindable/RasterizerState.h"

class Scene;

class Renderer
{
public:
	explicit Renderer(Graphics& gfx);
	void Render(Scene& scene, Camera* activeCamera) noexcept(!IS_DEBUG);

private:
	struct FrameLightCbuf
	{
		DirectX::XMFLOAT3 ambientColor = { 0.0f, 0.0f, 0.0f };
		std::uint32_t applyAmbient = 0u;
		std::uint32_t hasActiveLight = 0u;
		std::uint32_t lightType = 0u;
		DirectX::XMFLOAT2 padding = { 0.0f, 0.0f };
	};

	static_assert(sizeof(FrameLightCbuf) == 32u, "FrameLightCbuf must match shader layout.");

	RenderView BuildView(Camera* activeCamera) const noexcept;
	void ConfigurePassStates();
	void ExecutePassState(const PassState& state) noexcept;
	void ExecuteCallbacks(RenderPassId passId) noexcept(!IS_DEBUG);
	void ExecuteOpaqueBase(const RenderView& view) noexcept(!IS_DEBUG);
	void ExecuteOpaqueAccum(const RenderView& view) noexcept(!IS_DEBUG);
	void DrawOpaqueItem(const RenderItem& item) noexcept(!IS_DEBUG);
	void BindLighting(const RenderLight* light, bool applyAmbient) noexcept;
	const RenderLight* FindPrimaryDirectional(const std::vector<RenderLight>& lights) const noexcept;

private:
	struct LightPassCbuf
	{
		DirectX::XMFLOAT3 color = { 0.0f, 0.0f, 0.0f };
		float intensity = 0.0f;
		DirectX::XMFLOAT3 direction = { 0.0f, 0.0f, 1.0f };
		float attConst = 1.0f;
		DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
		float attLinear = 0.0f;
		float attQuad = 0.0f;
		std::uint32_t enabled = 0u;
		DirectX::XMFLOAT2 padding = { 0.0f, 0.0f };
	};

	static_assert(sizeof(LightPassCbuf) == 64u, "LightPassCbuf must match shader layout.");

	Graphics& gfx;
	RenderQueue queue;
	BlendState opaqueBlendState;
	BlendState additiveBlendState;
	DepthStencilState baseDepthState;
	DepthStencilState additiveDepthState;
	RasterizerState solidRasterizerState;
	PixelConstantBuffer<FrameLightCbuf> frameLightCbuf;
	PixelConstantBuffer<LightPassCbuf> lightPassCbuf;
};
