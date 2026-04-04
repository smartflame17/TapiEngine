#pragma once

#include "Graphics.h"
#include "RenderQueue.h"
#include "IBindable/BlendState.h"
#include "IBindable/ConstantBuffers.h"
#include "IBindable/DepthStencilState.h"
#include "IBindable/RasterizerState.h"
#include "IBindable/Sampler.h"
#include "IBindable/ShadowMap.h"
#include "IBindable/VertexShader.h"
#include <array>

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
	void ExecuteShadowPass(const RenderView& view) noexcept(!IS_DEBUG);
	void ExecuteOpaqueBase(const RenderView& view) noexcept(!IS_DEBUG);
	void ExecuteOpaqueAccum(const RenderView& view) noexcept(!IS_DEBUG);
	void DrawOpaqueItem(const RenderItem& item) noexcept(!IS_DEBUG);
	void BindLighting(const RenderLight* light, bool applyAmbient) noexcept;
	const RenderLight* FindPrimaryDirectional(const std::vector<RenderLight>& lights) const noexcept;
	const RenderLight* FindPrimaryPoint(const std::vector<RenderLight>& lights) const noexcept;
	const RenderLight* FindPrimarySpot(const std::vector<RenderLight>& lights) const noexcept;
	DirectX::XMMATRIX BuildSpotLightViewProjection(const RenderLight& light) const noexcept;
	std::array<DirectX::XMMATRIX, 6u> BuildPointLightViewProjections(const RenderLight& light) const noexcept;

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
		float innerConeCos = 1.0f;
		float outerConeCos = 1.0f;
		std::uint32_t enabled = 0u;
		//DirectX::XMFLOAT3 padding = { 0.0f, 0.0f, 0.0f };
	};

	static_assert(sizeof(LightPassCbuf) == 64u, "LightPassCbuf must match shader layout.");

	struct LightShadowCbuf
	{
		DirectX::XMFLOAT4X4 lightViewProjection[6] = {};
		DirectX::XMFLOAT3 shadowLightPosition = { 0.0f, 0.0f, 0.0f };
		float shadowStrength = 0.0f;
		DirectX::XMFLOAT2 shadowMapTexelSize = { 0.0f, 0.0f };
		std::uint32_t shadowEnabled = 0u;
		std::uint32_t shadowType = 0u;
	};

	static_assert(sizeof(LightShadowCbuf) == 416u, "LightShadowCbuf must match shader layout.");

	Graphics& gfx;
	RenderQueue queue;
	BlendState opaqueBlendState;
	BlendState additiveBlendState;
	DepthStencilState baseDepthState;
	DepthStencilState additiveDepthState;
	RasterizerState solidRasterizerState;
	DepthStencilState shadowDepthState;
	RasterizerState shadowRasterizerState;
	PixelConstantBuffer<FrameLightCbuf> frameLightCbuf;
	PixelConstantBuffer<LightPassCbuf> lightPassCbuf;
	PixelConstantBuffer<LightShadowCbuf> lightShadowCbuf; // for shadow pass (contains light view-projection matrix and shadow map parameters), do NOT use on main pass
	VertexShader shadowVertexShader;
	ShadowMap spotShadowMap;
	ShadowMap pointShadowMap;
	Sampler shadowSampler;
	std::array<DirectX::XMFLOAT4X4, 6u> activeSpotLightViewProjection = {};
	std::array<DirectX::XMFLOAT4X4, 6u> activePointLightViewProjection = {};
	bool hasActiveSpotLightShadow = false;
	bool hasActivePointLightShadow = false;
	const RenderLight* pShadowCastingPoint = nullptr;
	const RenderLight* pShadowCastingSpot = nullptr;
};

// TODO: consider using a structured buffer for lights instead of cBuffers, to allow more than 1 light and more flexible light count (currently we are limited by cBuffer size and we have to set an upper limit on light count)
// TODO: refactor shadow pass to be more flexible and support multiple shadow-casting lights (currently we only support 1 shadow-casting directional light and 1 shadow-casting spot light, and we have to choose which one is the "primary" light that casts shadows if both are present)
