#pragma once

#include <DirectXCollision.h>
#include "SimpleMath.h"
#include <cstdint>
#include <memory>
#include <vector>

enum class ProxyType : std::uint8_t
{
	Unknown = 0,
	Collider,
	Renderable,
	Light,
	Camera
};

struct SpatialProxy
{
	DirectX::BoundingBox bounds;
	void* userData = nullptr;
	std::uint32_t layerMask = 0xFFFFFFFFu;
	ProxyType type = ProxyType::Unknown;
	bool isStatic = false;
};

struct BVHNode
{
	DirectX::BoundingBox bounds;
	BVHNode* left = nullptr;
	BVHNode* right = nullptr;
	BVHNode* parent = nullptr;
	std::vector<SpatialProxy*> objects;
	bool isLeaf = true;
};

class BVH
{
public:
	explicit BVH(std::size_t maxLeafObjects = 4u);

	SpatialProxy* Insert(const SpatialProxy& proxy);
	void Remove(SpatialProxy* proxy);
	void Update(SpatialProxy* proxy);

	void Build();
	void Refit();
	void Clear() noexcept;

	void QueryAABB(const DirectX::BoundingBox& region, std::vector<SpatialProxy*>& results, std::uint32_t layerMask = 0xFFFFFFFFu, ProxyType typeFilter = ProxyType::Unknown) const;
	void Raycast(const DirectX::SimpleMath::Ray& ray, std::vector<SpatialProxy*>& results, std::uint32_t layerMask = 0xFFFFFFFFu, ProxyType typeFilter = ProxyType::Unknown) const;
	void FrustumQuery(const DirectX::BoundingFrustum& frustum, std::vector<SpatialProxy*>& results, std::uint32_t layerMask = 0xFFFFFFFFu, ProxyType typeFilter = ProxyType::Unknown) const;

private:
	class NodePool
	{
	public:
		BVHNode* Acquire();
		void Reset() noexcept;

	private:
		std::vector<std::unique_ptr<BVHNode>> nodes;
		std::size_t nextFree = 0u;
	};

private:
	BVHNode* BuildRecursive(std::vector<SpatialProxy*>& objects, BVHNode* parent);
	void QueryAABBRecursive(BVHNode* node, const DirectX::BoundingBox& region, std::vector<SpatialProxy*>& results, std::uint32_t layerMask, ProxyType typeFilter) const;
	void RaycastRecursive(BVHNode* node, const DirectX::SimpleMath::Ray& ray, std::vector<SpatialProxy*>& results, std::uint32_t layerMask, ProxyType typeFilter) const;
	void FrustumQueryRecursive(BVHNode* node, const DirectX::BoundingFrustum& frustum, std::vector<SpatialProxy*>& results, std::uint32_t layerMask, ProxyType typeFilter) const;
	void RefitRecursive(BVHNode* node);
	static bool PassesFilter(const SpatialProxy& proxy, std::uint32_t layerMask, ProxyType typeFilter) noexcept;

private:
	NodePool nodePool;
	BVHNode* root = nullptr;
	std::vector<std::unique_ptr<SpatialProxy>> proxies;
	std::size_t maxLeafObjects = 4u;
	bool needsBuild = false;
};
