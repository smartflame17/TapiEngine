#include "Renderer.h"
#include "Camera.h"
#include "Drawable/Drawable.h"
#include "IBindable/ShadowTransformCbuf.h"
#include "../Scene/Scene.h"
#include <algorithm>
#include <cmath>

namespace
{
	D3D11_BLEND_DESC MakeOpaqueBlendDesc() noexcept
	{
		D3D11_BLEND_DESC desc = {};
		desc.AlphaToCoverageEnable = FALSE;
		desc.IndependentBlendEnable = FALSE;
		auto& rt = desc.RenderTarget[0];
		rt.BlendEnable = FALSE;
		rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		return desc;
	}

	D3D11_BLEND_DESC MakeAdditiveBlendDesc() noexcept
	{
		D3D11_BLEND_DESC desc = {};
		desc.AlphaToCoverageEnable = FALSE;
		desc.IndependentBlendEnable = FALSE;
		auto& rt = desc.RenderTarget[0];
		rt.BlendEnable = TRUE;
		rt.SrcBlend = D3D11_BLEND_ONE;
		rt.DestBlend = D3D11_BLEND_ONE;
		rt.BlendOp = D3D11_BLEND_OP_ADD;
		rt.SrcBlendAlpha = D3D11_BLEND_ONE;
		rt.DestBlendAlpha = D3D11_BLEND_ZERO;
		rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;
		rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		return desc;
	}

	D3D11_RASTERIZER_DESC MakeShadowRasterizerDesc() noexcept
	{
		D3D11_RASTERIZER_DESC desc = {};
		desc.FillMode = D3D11_FILL_SOLID;
		desc.CullMode = D3D11_CULL_BACK;
		desc.FrontCounterClockwise = FALSE;
		desc.DepthClipEnable = TRUE;
		desc.DepthBias = 1000;
		desc.SlopeScaledDepthBias = 2.0f;
		desc.DepthBiasClamp = 0.0f;
		return desc;
	}
}

Renderer::Renderer(Graphics& gfx)
	:
	gfx(gfx),
	opaqueBlendState(gfx, MakeOpaqueBlendDesc()),
	additiveBlendState(gfx, MakeAdditiveBlendDesc()),
	baseDepthState(gfx, true, D3D11_DEPTH_WRITE_MASK_ALL, D3D11_COMPARISON_LESS_EQUAL),
	additiveDepthState(gfx, true, D3D11_DEPTH_WRITE_MASK_ZERO, D3D11_COMPARISON_EQUAL),
	solidRasterizerState(gfx, D3D11_CULL_BACK),
	shadowDepthState(gfx, true, D3D11_DEPTH_WRITE_MASK_ALL, D3D11_COMPARISON_LESS_EQUAL),
	shadowRasterizerState(gfx, MakeShadowRasterizerDesc()),
	frameLightCbuf(gfx, 1u),
	lightPassCbuf(gfx, 3u),
	lightShadowCbuf(gfx, 4u),
	shadowVertexShader(gfx, L"ShadowMapVS.cso"),
	shadowMap(gfx, 2048u, 2u),
	shadowSampler(gfx, Sampler::Type::LinearClamp, 1u)
{
}

void Renderer::Render(Scene& scene, Camera* activeCamera) noexcept(!IS_DEBUG)
{
	queue.Reset();

	auto view = BuildView(activeCamera);
	scene.CollectRenderLights(view.lights);
	RenderQueueBuilder builder(queue, view);
	scene.Submit(builder, view);
	ConfigurePassStates();
	queue.Sort();

	ExecuteCallbacks(RenderPassId::Skybox);
	ExecuteShadowPass(view);
	ExecuteOpaqueBase(view);
	ExecuteOpaqueAccum(view);
	ExecuteCallbacks(RenderPassId::EditorGizmos);
	shadowMap.Unbind(gfx);
	gfx.RestoreDefaultStates();

	const auto& wireframeSettings = gfx.GetWireframeDebugSettings();
	if (wireframeSettings.enabled)
	{
		std::vector<DirectX::BoundingBox> hierarchyBounds;
		scene.CollectBVHBounds(hierarchyBounds);
		gfx.DrawWireframeBoundingBoxes(hierarchyBounds);
	}
}

RenderView Renderer::BuildView(Camera* activeCamera) const noexcept
{
	RenderView view;
	view.camera = activeCamera;
	view.view = activeCamera != nullptr ? activeCamera->GetViewMatrix() : DirectX::XMMatrixIdentity();
	view.projection = gfx.GetProjection();
	view.viewportWidth = gfx.GetViewportWidth();
	view.viewportHeight = gfx.GetViewportHeight();
	view.viewportTopLeftX = gfx.GetViewportTopLeftX();
	view.viewportTopLeftY = gfx.GetViewportTopLeftY();

	if (activeCamera != nullptr)
	{
		const auto inverseCamera = DirectX::XMMatrixInverse(nullptr, view.view);
		view.cameraPosition = {
			DirectX::XMVectorGetX(inverseCamera.r[3]),
			DirectX::XMVectorGetY(inverseCamera.r[3]),
			DirectX::XMVectorGetZ(inverseCamera.r[3])
		};
	}

	return view;
}

void Renderer::ConfigurePassStates()
{
	queue.GetPassState(RenderPassId::Skybox) = { &opaqueBlendState, nullptr, nullptr, false, false, false };
	queue.GetPassState(RenderPassId::ShadowMap) = { &opaqueBlendState, &shadowDepthState, &shadowRasterizerState, false, true, false };
	queue.GetPassState(RenderPassId::OpaqueBase) = { &opaqueBlendState, &baseDepthState, &solidRasterizerState, false, false, false };
	queue.GetPassState(RenderPassId::OpaqueLightAccum) = { &additiveBlendState, &additiveDepthState, &solidRasterizerState, false, false, false };
	queue.GetPassState(RenderPassId::EditorGizmos) = { &opaqueBlendState, &baseDepthState, &solidRasterizerState, false, false, false };
}

void Renderer::ExecutePassState(const PassState& state) noexcept
{
	gfx.BindMainRenderTarget();
	if (state.blendState != nullptr)
	{
		state.blendState->Bind(gfx);
	}
	if (state.depthStencilState != nullptr)
	{
		state.depthStencilState->Bind(gfx);
	}
	if (state.rasterizerState != nullptr)
	{
		state.rasterizerState->Bind(gfx);
	}
}

void Renderer::ExecuteCallbacks(RenderPassId passId) noexcept(!IS_DEBUG)
{
	ExecutePassState(queue.GetPassState(passId));
	for (const auto& item : queue.GetPassItems(passId))
	{
		if (item.callback)
		{
			item.callback(gfx);
		}
	}
	gfx.RestoreDefaultStates();
}

void Renderer::ExecuteShadowPass(const RenderView& view) noexcept(!IS_DEBUG)
{
	pShadowCastingSpot = FindPrimarySpot(view.lights);
	hasActiveSpotLightShadow = false;

	if (pShadowCastingSpot == nullptr)
	{
		return;
	}

	DirectX::XMStoreFloat4x4(&activeSpotLightViewProjection, BuildSpotLightViewProjection(*pShadowCastingSpot));
	ShadowTransformCbuf::SetLightViewProjection(DirectX::XMLoadFloat4x4(&activeSpotLightViewProjection));
	hasActiveSpotLightShadow = true;
	shadowMap.BeginWrite(gfx);
	shadowDepthState.Bind(gfx);
	shadowRasterizerState.Bind(gfx);
	shadowVertexShader.Bind(gfx);
	gfx.UnbindPixelShader();

	for (const auto& item : queue.GetOpaqueItems())
	{
		if (item.drawable == nullptr)
		{
			continue;
		}

		item.drawable->SetExternalTransformMatrix(DirectX::XMLoadFloat4x4(&item.transform));
		item.drawable->DrawShadow(gfx, shadowVertexShader.GetBytecode());
	}

	gfx.BindMainRenderTarget();
	gfx.RestoreDefaultStates();
}

void Renderer::ExecuteOpaqueBase(const RenderView& view) noexcept(!IS_DEBUG)
{
	ExecutePassState(queue.GetPassState(RenderPassId::OpaqueBase));
	const auto* primaryDirectional = FindPrimaryDirectional(view.lights);
	BindLighting(primaryDirectional, true);

	for (const auto& item : queue.GetOpaqueItems())
	{
		DrawOpaqueItem(item);
	}

	gfx.RestoreDefaultStates();
}

void Renderer::ExecuteOpaqueAccum(const RenderView& view) noexcept(!IS_DEBUG)
{
	ExecutePassState(queue.GetPassState(RenderPassId::OpaqueLightAccum));
	const auto* primaryDirectional = FindPrimaryDirectional(view.lights);
	for (const auto& light : view.lights)
	{
		if (!light.enabled)
		{
			continue;
		}
		if (primaryDirectional != nullptr && &light == primaryDirectional)
		{
			continue;
		}

		BindLighting(&light, false);
		for (const auto& item : queue.GetOpaqueItems())
		{
			DrawOpaqueItem(item);
		}
	}

	gfx.RestoreDefaultStates();
}

void Renderer::DrawOpaqueItem(const RenderItem& item) noexcept(!IS_DEBUG)
{
	if (item.drawable == nullptr)
	{
		return;
	}

	item.drawable->SetExternalTransformMatrix(DirectX::XMLoadFloat4x4(&item.transform));
	item.drawable->Draw(gfx);
}

void Renderer::BindLighting(const RenderLight* light, bool applyAmbient) noexcept
{
	FrameLightCbuf frameData;
	frameData.ambientColor = applyAmbient ? DirectX::XMFLOAT3{ 0.6f, 0.6f, 0.6f } : DirectX::XMFLOAT3{ 0.0f, 0.0f, 0.0f };
	frameData.applyAmbient = applyAmbient ? 1u : 0u;
	frameData.hasActiveLight = (light != nullptr && light->enabled) ? 1u : 0u;
	frameData.lightType = light != nullptr ? static_cast<std::uint32_t>(light->type) : 0u;
	frameLightCbuf.Update(gfx, frameData);
	frameLightCbuf.Bind(gfx);

	LightPassCbuf passData;
	if (light != nullptr)
	{
		passData.color = light->color;
		passData.intensity = light->intensity;
		passData.direction = light->direction;
		passData.attConst = light->attConst;
		passData.position = light->position;
		passData.attLinear = light->attLinear;
		passData.attQuad = light->attQuad;
		passData.innerConeCos = light->innerConeCos;
		passData.outerConeCos = light->outerConeCos;
		passData.enabled = light->enabled;
	}
	lightPassCbuf.Update(gfx, passData);
	lightPassCbuf.Bind(gfx);

	LightShadowCbuf shadowData;
	const bool enableShadow = light != nullptr &&
		light->enabled != 0u &&
		light->type == LightType::Spot &&
		pShadowCastingSpot != nullptr &&
		light == pShadowCastingSpot &&
		hasActiveSpotLightShadow;
	if (enableShadow)
	{
		DirectX::XMStoreFloat4x4(&shadowData.lightViewProjection, DirectX::XMMatrixTranspose(DirectX::XMLoadFloat4x4(&activeSpotLightViewProjection)));
		shadowData.shadowMapTexelSize = { 1.0f / 2048.0f, 1.0f / 2048.0f };
		shadowData.shadowEnabled = 1u;
		shadowData.shadowStrength = 0.65f;
		shadowMap.Bind(gfx);
		shadowSampler.Bind(gfx);
	}
	lightShadowCbuf.Update(gfx, shadowData);
	lightShadowCbuf.Bind(gfx);
}

const RenderLight* Renderer::FindPrimaryDirectional(const std::vector<RenderLight>& lights) const noexcept
{
	for (const auto& light : lights)
	{
		if (light.enabled && light.type == LightType::Directional)
		{
			return &light;
		}
	}
	return nullptr;
}

const RenderLight* Renderer::FindPrimarySpot(const std::vector<RenderLight>& lights) const noexcept
{
	for (const auto& light : lights)
	{
		if (light.enabled && light.type == LightType::Spot)
		{
			return &light;
		}
	}
	return nullptr;
}

DirectX::XMMATRIX Renderer::BuildSpotLightViewProjection(const RenderLight& light) const noexcept
{
	const auto position = DirectX::XMLoadFloat3(&light.position);
	const auto direction = DirectX::XMLoadFloat3(&light.direction);
	DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	const float alignment = std::abs(DirectX::XMVectorGetX(DirectX::XMVector3Dot(direction, up)));
	if (alignment > 0.99f)
	{
		up = DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	}

	const auto view = DirectX::XMMatrixLookToLH(position, direction, up);
	const float fov = std::clamp(2.0f * std::acos(std::clamp(light.outerConeCos, -1.0f, 1.0f)), 0.1f, DirectX::XM_PI - 0.1f);
	const auto projection = DirectX::XMMatrixPerspectiveFovLH(fov, 1.0f, 0.1f, 100.0f);
	return view * projection;
}
