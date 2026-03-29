#include "Renderer.h"
#include "Camera.h"
#include "Drawable/Drawable.h"
#include "../Scene/Scene.h"

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
}

Renderer::Renderer(Graphics& gfx)
	:
	gfx(gfx),
	opaqueBlendState(gfx, MakeOpaqueBlendDesc()),
	additiveBlendState(gfx, MakeAdditiveBlendDesc()),
	baseDepthState(gfx, true, D3D11_DEPTH_WRITE_MASK_ALL, D3D11_COMPARISON_LESS_EQUAL),
	additiveDepthState(gfx, true, D3D11_DEPTH_WRITE_MASK_ZERO, D3D11_COMPARISON_EQUAL),
	solidRasterizerState(gfx, D3D11_CULL_BACK),
	frameLightCbuf(gfx, 1u),
	lightPassCbuf(gfx, 3u)
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
	ExecuteOpaqueBase(view);
	ExecuteOpaqueAccum(view);
	ExecuteCallbacks(RenderPassId::EditorGizmos);
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
		passData.enabled = light->enabled;
	}
	lightPassCbuf.Update(gfx, passData);
	lightPassCbuf.Bind(gfx);
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
