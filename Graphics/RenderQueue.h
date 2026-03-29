#pragma once

#include "Lighting/RenderLight.h"
#include <DirectXMath.h>
#include <array>
#include <cstdint>
#include <functional>
#include <vector>

class Graphics;
class Drawable;
class Camera;
class BlendState;
class DepthStencilState;
class RasterizerState;

enum class RenderPassId : std::uint32_t
{
	Skybox = 0u,
	OpaqueBase,
	OpaqueLightAccum,
	EditorGizmos,
	Ui,
	StencilMask,
	Outline,
	Count
};

struct RenderView
{
	const Camera* camera = nullptr;
	DirectX::XMMATRIX view = DirectX::XMMatrixIdentity();
	DirectX::XMMATRIX projection = DirectX::XMMatrixIdentity();
	DirectX::XMFLOAT3 cameraPosition = { 0.0f, 0.0f, 0.0f };
	float viewportWidth = 0.0f;
	float viewportHeight = 0.0f;
	float viewportTopLeftX = 0.0f;
	float viewportTopLeftY = 0.0f;
	std::vector<RenderLight> lights;
};

struct PassState
{
	BlendState* blendState = nullptr;
	DepthStencilState* depthStencilState = nullptr;
	RasterizerState* rasterizerState = nullptr;
	bool clearColor = false;
	bool clearDepth = false;
	bool clearStencil = false;
};

struct RenderItem
{
	RenderPassId passId = RenderPassId::OpaqueBase;
	std::uint64_t sortKey = 0u;
	const Drawable* drawable = nullptr;
	DirectX::XMFLOAT4X4 transform = {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	};
	std::function<void(Graphics&)> callback;
};

class RenderQueue
{
public:
	using PassItems = std::array<std::vector<RenderItem>, static_cast<std::size_t>(RenderPassId::Count)>;
	using PassStates = std::array<PassState, static_cast<std::size_t>(RenderPassId::Count)>;

	void Reset() noexcept;
	std::vector<RenderItem>& GetOpaqueItems() noexcept;
	const std::vector<RenderItem>& GetOpaqueItems() const noexcept;
	std::vector<RenderItem>& GetPassItems(RenderPassId passId) noexcept;
	const std::vector<RenderItem>& GetPassItems(RenderPassId passId) const noexcept;
	PassState& GetPassState(RenderPassId passId) noexcept;
	const PassState& GetPassState(RenderPassId passId) const noexcept;
	void Sort() noexcept;

private:
	std::vector<RenderItem> opaqueItems;
	PassItems passItems;
	PassStates passStates;
};

class RenderQueueBuilder
{
public:
	RenderQueueBuilder(RenderQueue& queue, const RenderView& view) noexcept;

	void SubmitOpaque(const Drawable& drawable, DirectX::FXMMATRIX transform) noexcept;
	void SubmitCallback(RenderPassId passId, std::function<void(Graphics&)> callback, std::uint64_t sortKey = 0u);

private:
	std::uint64_t BuildOpaqueSortKey(DirectX::FXMMATRIX transform) const noexcept;

private:
	RenderQueue& queue;
	const RenderView& view;
};
