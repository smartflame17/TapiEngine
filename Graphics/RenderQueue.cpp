#include "RenderQueue.h"
#include "Drawable/Drawable.h"
#include <algorithm>
#include <cmath>

namespace
{
	constexpr std::size_t PassIndex(RenderPassId passId) noexcept
	{
		return static_cast<std::size_t>(passId);
	}
}

void RenderQueue::Reset() noexcept
{
	opaqueItems.clear();
	for (auto& items : passItems)
	{
		items.clear();
	}
	for (auto& state : passStates)
	{
		state = {};
	}
}

std::vector<RenderItem>& RenderQueue::GetOpaqueItems() noexcept
{
	return opaqueItems;
}

const std::vector<RenderItem>& RenderQueue::GetOpaqueItems() const noexcept
{
	return opaqueItems;
}

std::vector<RenderItem>& RenderQueue::GetPassItems(RenderPassId passId) noexcept
{
	return passItems[PassIndex(passId)];
}

const std::vector<RenderItem>& RenderQueue::GetPassItems(RenderPassId passId) const noexcept
{
	return passItems[PassIndex(passId)];
}

PassState& RenderQueue::GetPassState(RenderPassId passId) noexcept
{
	return passStates[PassIndex(passId)];
}

const PassState& RenderQueue::GetPassState(RenderPassId passId) const noexcept
{
	return passStates[PassIndex(passId)];
}

void RenderQueue::Sort() noexcept
{
	auto sortItems = [](auto& items)
	{
		std::sort(items.begin(), items.end(), [](const RenderItem& lhs, const RenderItem& rhs)
			{
				return lhs.sortKey < rhs.sortKey;
			});
	};

	sortItems(opaqueItems);
	for (auto& items : passItems)
	{
		sortItems(items);
	}
}

RenderQueueBuilder::RenderQueueBuilder(RenderQueue& queue, const RenderView& view) noexcept
	:
	queue(queue),
	view(view)
{
}

void RenderQueueBuilder::SubmitOpaque(const Drawable& drawable, DirectX::FXMMATRIX transform) noexcept
{
	RenderItem item;
	item.passId = RenderPassId::OpaqueBase;
	item.sortKey = BuildOpaqueSortKey(transform);
	item.drawable = &drawable;
	DirectX::XMStoreFloat4x4(&item.transform, transform);
	queue.GetOpaqueItems().push_back(std::move(item));
}

void RenderQueueBuilder::SubmitCallback(RenderPassId passId, std::function<void(Graphics&)> callback, std::uint64_t sortKey)
{
	RenderItem item;
	item.passId = passId;
	item.sortKey = sortKey;
	item.callback = std::move(callback);
	queue.GetPassItems(passId).push_back(std::move(item));
}

std::uint64_t RenderQueueBuilder::BuildOpaqueSortKey(DirectX::FXMMATRIX transform) const noexcept
{
	if (view.camera == nullptr)
	{
		return 0u;
	}

	DirectX::XMFLOAT4X4 world;
	DirectX::XMStoreFloat4x4(&world, transform);
	const float dx = world._41 - view.cameraPosition.x;
	const float dy = world._42 - view.cameraPosition.y;
	const float dz = world._43 - view.cameraPosition.z;
	const float distanceSq = dx * dx + dy * dy + dz * dz;
	const auto quantized = static_cast<std::uint32_t>(std::clamp(distanceSq * 1024.0f, 0.0f, static_cast<float>(UINT32_MAX)));
	return quantized;
}
