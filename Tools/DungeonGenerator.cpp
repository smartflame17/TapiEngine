#include "DungeonGenerator.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <limits>
#include <numeric>
#include <queue>
#include <random>
#include <sstream>

namespace
{
	constexpr char WallCell = '1';
	constexpr char OpenCell = '0';
}

DungeonGenerator::DungeonGenerator()
	: DungeonGenerator(std::random_device{}())
{}

DungeonGenerator::DungeonGenerator(unsigned int seed)
	: seed(seed)
{}

std::vector<std::string> DungeonGenerator::Generate(int width, int height, const GenerationSettings& settings)
{
	if (width <= 0 || height <= 0)
	{
		grid.clear();
		rooms.clear();
		bspNodes.clear();
		return grid;
	}

	grid.assign(height, std::string(width, WallCell));
	rooms.clear();
	bspNodes.clear();

	if (width < 3 || height < 3)
	{
		return grid;
	}

	Rect rootRegion{ 1, 1, width - 2, height - 2 };
	bspNodes.push_back({ rootRegion });

	std::vector<int> stack;
	stack.push_back(0);
	while (!stack.empty())
	{
		const int nodeIndex = stack.back();
		stack.pop_back();

		if (SplitNode(nodeIndex, settings))
		{
			stack.push_back(bspNodes[nodeIndex].left);
			stack.push_back(bspNodes[nodeIndex].right);
		}
	}

	BuildRoomsFromLeaves(0, settings);
	for (const Room& room : rooms)
	{
		CarveRoom(room);
	}

	ConnectRooms();

	while (IsBoundaryExposed())
	{
		PadBoundaryWithWalls();
	}

	return grid;
}


std::vector<std::string> DungeonGenerator::Generate(int width, int height)
{
	return Generate(width, height, GenerationSettings{});
}
bool DungeonGenerator::SaveToFile(const std::string& filePath) const
{
	std::ofstream out(filePath);
	if (!out.is_open())
	{
		return false;
	}

	out << ToText();
	return true;
}

std::string DungeonGenerator::ToText() const
{
	const int width = grid.empty() ? 0 : static_cast<int>(grid.front().size());
	const int height = static_cast<int>(grid.size());

	std::ostringstream text;
	text << width << ' ' << height << '\n';
	for (const std::string& row : grid)
	{
		text << row << '\n';
	}
	return text.str();
}

const std::vector<std::string>& DungeonGenerator::GetGrid() const noexcept
{
	return grid;
}

bool DungeonGenerator::SplitNode(int nodeIndex, const GenerationSettings& settings)
{
	const Rect region = bspNodes[nodeIndex].region;

	const bool canSplitHorizontally = region.height >= settings.minLeafSize * 2;
	const bool canSplitVertically = region.width >= settings.minLeafSize * 2;
	if (region.width <= settings.maxLeafSize && region.height <= settings.maxLeafSize)
	{
		return false;
	}

	if (!canSplitHorizontally && !canSplitVertically)
	{
		return false;
	}

	std::mt19937 rng(seed + static_cast<unsigned int>(nodeIndex * 7919));
	std::uniform_real_distribution<float> axisDist(0.0f, 1.0f);
	bool splitVertical = axisDist(rng) < settings.splitBias;

	if (canSplitVertically && canSplitHorizontally)
	{
		if (region.width > region.height)
		{
			splitVertical = true;
		}
		else if (region.height > region.width)
		{
			splitVertical = false;
		}
	}
	else if (!canSplitVertically)
	{
		splitVertical = false;
	}
	else
	{
		splitVertical = true;
	}

	if (splitVertical)
	{
		const int minSplit = region.x + settings.minLeafSize;
		const int maxSplit = region.x + region.width - settings.minLeafSize;
		if (maxSplit <= minSplit)
		{
			return false;
		}

		std::uniform_int_distribution<int> splitDist(minSplit, maxSplit);
		const int splitX = splitDist(rng);
		const int leftIndex = static_cast<int>(bspNodes.size());
		bspNodes.push_back({ Rect{ region.x, region.y, splitX - region.x, region.height } });
		const int rightIndex = static_cast<int>(bspNodes.size());
		bspNodes.push_back({ Rect{ splitX, region.y, region.x + region.width - splitX, region.height } });
		bspNodes[nodeIndex].left = leftIndex;
		bspNodes[nodeIndex].right = rightIndex;
	}
	else
	{
		const int minSplit = region.y + settings.minLeafSize;
		const int maxSplit = region.y + region.height - settings.minLeafSize;
		if (maxSplit <= minSplit)
		{
			return false;
		}

		std::uniform_int_distribution<int> splitDist(minSplit, maxSplit);
		const int splitY = splitDist(rng);
		const int leftIndex = static_cast<int>(bspNodes.size());
		bspNodes.push_back({ Rect{ region.x, region.y, region.width, splitY - region.y } });
		const int rightIndex = static_cast<int>(bspNodes.size());
		bspNodes.push_back({ Rect{ region.x, splitY, region.width, region.y + region.height - splitY } });
		bspNodes[nodeIndex].left = leftIndex;
		bspNodes[nodeIndex].right = rightIndex;
	}

	return true;
}

void DungeonGenerator::BuildRoomsFromLeaves(int nodeIndex, const GenerationSettings& settings)
{
	if (nodeIndex < 0 || nodeIndex >= static_cast<int>(bspNodes.size()))
	{
		return;
	}

	const Node& node = bspNodes[nodeIndex];
	if (node.left >= 0 || node.right >= 0)
	{
		BuildRoomsFromLeaves(node.left, settings);
		BuildRoomsFromLeaves(node.right, settings);
		return;
	}

	Rect roomRect = node.region;
	roomRect.x += settings.roomPadding;
	roomRect.y += settings.roomPadding;
	roomRect.width -= settings.roomPadding * 2;
	roomRect.height -= settings.roomPadding * 2;
	if (roomRect.width < settings.minRoomSize || roomRect.height < settings.minRoomSize)
	{
		return;
	}

	std::mt19937 rng(seed + static_cast<unsigned int>(nodeIndex * 104729));
	std::uniform_int_distribution<int> roomWidthDist(settings.minRoomSize, roomRect.width);
	std::uniform_int_distribution<int> roomHeightDist(settings.minRoomSize, roomRect.height);
	const int roomWidth = roomWidthDist(rng);
	const int roomHeight = roomHeightDist(rng);

	std::uniform_int_distribution<int> roomXDist(roomRect.x, roomRect.x + roomRect.width - roomWidth);
	std::uniform_int_distribution<int> roomYDist(roomRect.y, roomRect.y + roomRect.height - roomHeight);
	const int roomX = roomXDist(rng);
	const int roomY = roomYDist(rng);

	Room room;
	room.bounds = { roomX, roomY, roomWidth, roomHeight };
	room.center = { roomX + roomWidth / 2, roomY + roomHeight / 2 };
	rooms.push_back(room);
}

void DungeonGenerator::CarveRoom(const Room& room)
{
	for (int y = room.bounds.y; y < room.bounds.y + room.bounds.height; ++y)
	{
		for (int x = room.bounds.x; x < room.bounds.x + room.bounds.width; ++x)
		{
			if (IsInBounds(x, y))
			{
				grid[y][x] = OpenCell;
			}
		}
	}
}

void DungeonGenerator::ConnectRooms()
{
	if (rooms.size() <= 1)
	{
		return;
	}

	const std::vector<std::pair<int, int>> mstEdges = BuildMinimumSpanningTree();
	for (const auto& edge : mstEdges)
	{
		const Room& a = rooms[edge.first];
		const Room& b = rooms[edge.second];
		const std::vector<Point> path = FindCorridorPathAStar(a.center, b.center);
		CarveCorridor(path);
	}
}

std::vector<std::pair<int, int>> DungeonGenerator::BuildMinimumSpanningTree() const
{
	std::vector<std::pair<int, int>> edges;
	if (rooms.size() <= 1)
	{
		return edges;
	}

	const int n = static_cast<int>(rooms.size());
	std::vector<int> minCost(n, std::numeric_limits<int>::max());
	std::vector<int> parent(n, -1);
	std::vector<bool> inTree(n, false);
	minCost[0] = 0;

	for (int i = 0; i < n; ++i)
	{
		int u = -1;
		for (int v = 0; v < n; ++v)
		{
			if (!inTree[v] && (u == -1 || minCost[v] < minCost[u]))
			{
				u = v;
			}
		}

		if (u == -1)
		{
			break;
		}

		inTree[u] = true;
		if (parent[u] != -1)
		{
			edges.push_back({ parent[u], u });
		}

		for (int v = 0; v < n; ++v)
		{
			if (inTree[v])
			{
				continue;
			}

			const int cost = std::abs(rooms[u].center.x - rooms[v].center.x) + std::abs(rooms[u].center.y - rooms[v].center.y);
			if (cost < minCost[v])
			{
				minCost[v] = cost;
				parent[v] = u;
			}
		}
	}

	return edges;
}

std::vector<DungeonGenerator::Point> DungeonGenerator::FindCorridorPathAStar(const Point& start, const Point& end) const
{
	struct SearchNode
	{
		Point p;
		float g = 0.0f;
		float f = 0.0f;
	};

	const int width = grid.empty() ? 0 : static_cast<int>(grid.front().size());
	const int height = static_cast<int>(grid.size());
	const int totalCells = width * height;
	if (totalCells == 0)
	{
		return {};
	}

	auto toIndex = [width](int x, int y)
	{
		return y * width + x;
	};

	auto heuristic = [&end](const Point& p)
	{
		return static_cast<float>(std::abs(p.x - end.x) + std::abs(p.y - end.y));
	};

	const std::array<Point, 4> directions = { Point{1,0}, Point{-1,0}, Point{0,1}, Point{0,-1} };
	std::vector<float> bestCost(totalCells, std::numeric_limits<float>::infinity());
	std::vector<int> cameFrom(totalCells, -1);

	struct Compare
	{
		bool operator()(const SearchNode& a, const SearchNode& b) const
		{
			return a.f > b.f;
		}
	};
	std::priority_queue<SearchNode, std::vector<SearchNode>, Compare> frontier;
	const int startIndex = toIndex(start.x, start.y);
	bestCost[startIndex] = 0.0f;
	frontier.push({ start, 0.0f, heuristic(start) });

	while (!frontier.empty())
	{
		const SearchNode current = frontier.top();
		frontier.pop();

		if (current.p.x == end.x && current.p.y == end.y)
		{
			std::vector<Point> path;
			int index = toIndex(end.x, end.y);
			while (index != -1)
			{
				const int x = index % width;
				const int y = index / width;
				path.push_back({ x, y });
				index = cameFrom[index];
			}
			std::reverse(path.begin(), path.end());
			return path;
		}

		const int currentIndex = toIndex(current.p.x, current.p.y);
		if (current.g > bestCost[currentIndex])
		{
			continue;
		}

		for (const Point& dir : directions)
		{
			const Point next{ current.p.x + dir.x, current.p.y + dir.y };
			if (!IsInBounds(next.x, next.y))
			{
				continue;
			}

			const int nextIndex = toIndex(next.x, next.y);
			const bool isExistingOpen = grid[next.y][next.x] == OpenCell;
			const float stepCost = isExistingOpen ? 0.35f : 1.0f;
			const float tentativeCost = current.g + stepCost;
			if (tentativeCost < bestCost[nextIndex])
			{
				bestCost[nextIndex] = tentativeCost;
				cameFrom[nextIndex] = currentIndex;
				frontier.push({ next, tentativeCost, tentativeCost + heuristic(next) });
			}
		}
	}

	return {};
}

void DungeonGenerator::CarveCorridor(const std::vector<Point>& path)
{
	for (const Point& p : path)
	{
		if (IsInBounds(p.x, p.y))
		{
			grid[p.y][p.x] = OpenCell;
		}
	}
}

bool DungeonGenerator::IsInBounds(int x, int y) const noexcept
{
	return y >= 0 && y < static_cast<int>(grid.size()) && x >= 0 && x < static_cast<int>(grid[y].size());
}

bool DungeonGenerator::IsBoundaryExposed() const
{
	if (grid.empty() || grid.front().empty())
	{
		return false;
	}

	const int width = static_cast<int>(grid.front().size());
	const int height = static_cast<int>(grid.size());
	for (int x = 0; x < width; ++x)
	{
		if (grid[0][x] == OpenCell || grid[height - 1][x] == OpenCell)
		{
			return true;
		}
	}

	for (int y = 0; y < height; ++y)
	{
		if (grid[y][0] == OpenCell || grid[y][width - 1] == OpenCell)
		{
			return true;
		}
	}
	return false;
}

void DungeonGenerator::PadBoundaryWithWalls()
{
	if (grid.empty())
	{
		return;
	}

	const int oldWidth = static_cast<int>(grid.front().size());
	std::vector<std::string> padded;
	padded.reserve(grid.size() + 2);
	padded.push_back(std::string(oldWidth + 2, WallCell));
	for (const std::string& row : grid)
	{
		padded.push_back(std::string(1, WallCell) + row + std::string(1, WallCell));
	}
	padded.push_back(std::string(oldWidth + 2, WallCell));
	grid = std::move(padded);
}
