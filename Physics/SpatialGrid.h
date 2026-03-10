#pragma once
#include "Physics.h"
#include <vector>
#include <unordered_map>

namespace Physics
{
    // A hash for 3D grid coordinates
    struct GridKey
    {
        int x, y, z;
        bool operator==(const GridKey& other) const { return x == other.x && y == other.y && z == other.z; }
    };

    struct GridKeyHash
    {
        std::size_t operator()(const GridKey& k) const
        {
            return ((std::hash<int>()(k.x) ^ (std::hash<int>()(k.y) << 1)) >> 1) ^ (std::hash<int>()(k.z) << 1);
        }
    };

    class SpatialGrid
    {
    public:
        SpatialGrid(float cellSize) : cellSize(cellSize) {}

        void Clear()
        {
            grid.clear();
        }

        void Insert(Collider* collider);

        // Returns potential collision candidates (Broadphase)
        std::vector<Collider*> GetCandidates(Collider* collider);

    private:
        float cellSize;
        std::unordered_map<GridKey, std::vector<Collider*>, GridKeyHash> grid;
    };
}