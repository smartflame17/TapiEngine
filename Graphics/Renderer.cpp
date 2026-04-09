#include "Renderer.h"
#include "Camera.h"
#include "Drawable/Drawable.h"
#include "IBindable/ShadowTransformCbuf.h"
#include "Lighting/DirectionalLight.h"
#include "../Scene/Scene.h"
#include <algorithm>
#include <cmath>

namespace
{
	constexpr UINT kShadowMapSize = 2048u;
	constexpr UINT kDirectionalShadowMapSlot = 4u;
	constexpr UINT kSpotShadowMapSlot = 2u;
	constexpr UINT kPointShadowMapSlot = 3u;
	constexpr UINT kPointShadowFaceCount = 6u;
	constexpr float kShadowStrength = 0.65f;
	constexpr float kShadowNearPlane = 0.1f;
	constexpr float kShadowFarPlane = 100.0f;

	enum class ShadowType : std::uint32_t
	{
		None = 0u,
		Spot = 1u,
		Point = 2u,
		Directional = 3u
	};

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
	directionalShadowMap(gfx, kShadowMapSize, kDirectionalShadowMapSlot, ShadowMap::Type::Texture2D),
	spotShadowMap(gfx, kShadowMapSize, kSpotShadowMapSlot, ShadowMap::Type::Texture2D),
	pointShadowMap(gfx, kShadowMapSize, kPointShadowMapSlot, ShadowMap::Type::TextureCube),
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
	directionalShadowMap.Unbind(gfx);
	spotShadowMap.Unbind(gfx);
	pointShadowMap.Unbind(gfx);
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
	pShadowCastingPoint = FindPrimaryPoint(view.lights);
	pShadowCastingDirectional = FindPrimaryDirectional(view.lights);
	hasActiveDirectionalShadow = false;
	hasActiveSpotLightShadow = false;
	hasActivePointLightShadow = false;

	if (pShadowCastingDirectional == nullptr && pShadowCastingSpot == nullptr && pShadowCastingPoint == nullptr)
	{
		return;
	}

	shadowDepthState.Bind(gfx);
	shadowRasterizerState.Bind(gfx);
	shadowVertexShader.Bind(gfx);
	gfx.UnbindPixelShader();

	const auto drawShadowCasters = [this]()
	{
		for (const auto& item : queue.GetOpaqueItems())
		{
			if (item.drawable == nullptr)
			{
				continue;
			}

			item.drawable->SetExternalTransformMatrix(DirectX::XMLoadFloat4x4(&item.transform));
			item.drawable->DrawShadow(gfx, shadowVertexShader.GetBytecode());
		}
	};

	if (pShadowCastingDirectional != nullptr && view.camera != nullptr && pShadowCastingDirectional->pDirectionalLight != nullptr)
	{
		const auto directionalViewProjection = pShadowCastingDirectional->pDirectionalLight->GetLightViewProjection(view.camera->GetFrustum());
		DirectX::XMStoreFloat4x4(&activeDirectionalLightViewProjection, directionalViewProjection);
		ShadowTransformCbuf::SetLightViewProjection(directionalViewProjection);
		hasActiveDirectionalShadow = true;
		directionalShadowMap.BeginWrite(gfx);
		drawShadowCasters();
	}

	if (pShadowCastingSpot != nullptr)
	{
		const auto spotViewProjection = BuildSpotLightViewProjection(*pShadowCastingSpot);
		DirectX::XMStoreFloat4x4(&activeSpotLightViewProjection[0], spotViewProjection);
		ShadowTransformCbuf::SetLightViewProjection(spotViewProjection);
		hasActiveSpotLightShadow = true;
		spotShadowMap.BeginWrite(gfx);
		drawShadowCasters();
	}

	if (pShadowCastingPoint != nullptr)
	{
		const auto pointViewProjections = BuildPointLightViewProjections(*pShadowCastingPoint);
		hasActivePointLightShadow = true;

		for (UINT face = 0u; face < kPointShadowFaceCount; face++)
		{
			DirectX::XMStoreFloat4x4(&activePointLightViewProjection[face], pointViewProjections[face]);
			ShadowTransformCbuf::SetLightViewProjection(pointViewProjections[face]);
			pointShadowMap.BeginWriteFace(gfx, face);
			drawShadowCasters();
		}
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
	const bool enableSpotShadow = light != nullptr &&
		light->enabled != 0u &&
		light->type == LightType::Spot &&
		pShadowCastingSpot != nullptr &&
		light == pShadowCastingSpot &&
		hasActiveSpotLightShadow;
	const bool enableDirectionalShadow = light != nullptr &&
		light->enabled != 0u &&
		light->type == LightType::Directional &&
		pShadowCastingDirectional != nullptr &&
		light == pShadowCastingDirectional &&
		hasActiveDirectionalShadow;
	const bool enablePointShadow = light != nullptr &&
		light->enabled != 0u &&
		light->type == LightType::Point &&
		pShadowCastingPoint != nullptr &&
		light == pShadowCastingPoint &&
		hasActivePointLightShadow;
	if (enableDirectionalShadow)
	{
		DirectX::XMStoreFloat4x4(&shadowData.lightViewProjection[0], DirectX::XMMatrixTranspose(DirectX::XMLoadFloat4x4(&activeDirectionalLightViewProjection)));
		shadowData.shadowMapTexelSize = { 1.0f / static_cast<float>(kShadowMapSize), 1.0f / static_cast<float>(kShadowMapSize) };
		shadowData.shadowEnabled = 1u;
		shadowData.shadowType = static_cast<std::uint32_t>(ShadowType::Directional);
		shadowData.shadowStrength = kShadowStrength;
		directionalShadowMap.Bind(gfx);
		shadowSampler.Bind(gfx);
	}
	else if (enableSpotShadow)
	{
		DirectX::XMStoreFloat4x4(&shadowData.lightViewProjection[0], DirectX::XMMatrixTranspose(DirectX::XMLoadFloat4x4(&activeSpotLightViewProjection[0])));
		shadowData.shadowLightPosition = light->position;
		shadowData.shadowMapTexelSize = { 1.0f / static_cast<float>(kShadowMapSize), 1.0f / static_cast<float>(kShadowMapSize) };
		shadowData.shadowEnabled = 1u;
		shadowData.shadowType = static_cast<std::uint32_t>(ShadowType::Spot);
		shadowData.shadowStrength = kShadowStrength;
		spotShadowMap.Bind(gfx);
		shadowSampler.Bind(gfx);
	}
	else if (enablePointShadow)
	{
		for (UINT face = 0u; face < kPointShadowFaceCount; face++)
		{
			DirectX::XMStoreFloat4x4(&shadowData.lightViewProjection[face], DirectX::XMMatrixTranspose(DirectX::XMLoadFloat4x4(&activePointLightViewProjection[face])));
		}
		shadowData.shadowLightPosition = light->position;
		shadowData.shadowEnabled = 1u;
		shadowData.shadowType = static_cast<std::uint32_t>(ShadowType::Point);
		shadowData.shadowStrength = kShadowStrength;
		pointShadowMap.Bind(gfx);
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

const RenderLight* Renderer::FindPrimaryPoint(const std::vector<RenderLight>& lights) const noexcept
{
	for (const auto& light : lights)
	{
		if (light.enabled && light.type == LightType::Point)
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
	const auto projection = DirectX::XMMatrixPerspectiveFovLH(fov, 1.0f, kShadowNearPlane, kShadowFarPlane);
	return view * projection;
}

std::array<DirectX::XMMATRIX, 6u> Renderer::BuildPointLightViewProjections(const RenderLight& light) const noexcept
{
	const auto position = DirectX::XMLoadFloat3(&light.position);
	const std::array<DirectX::XMVECTOR, kPointShadowFaceCount> directions = {
		DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f),
		DirectX::XMVectorSet(-1.0f, 0.0f, 0.0f, 0.0f),
		DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f),
		DirectX::XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f),
		DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f),
		DirectX::XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f)
	};
	const std::array<DirectX::XMVECTOR, kPointShadowFaceCount> upVectors = {
		DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f),
		DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f),
		DirectX::XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f),
		DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f),
		DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f),
		DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)
	};

	std::array<DirectX::XMMATRIX, 6u> viewProjections = {};
	const auto projection = DirectX::XMMatrixPerspectiveFovLH(DirectX::XM_PIDIV2, 1.0f, kShadowNearPlane, kShadowFarPlane);

	for (UINT face = 0u; face < kPointShadowFaceCount; face++)
	{
		viewProjections[face] = DirectX::XMMatrixLookToLH(position, directions[face], upVectors[face]) * projection;
	}

	return viewProjections;
}
