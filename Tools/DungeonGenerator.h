#pragma once

#include <string>
#include <vector>

class DungeonGenerator
{
public:
	struct GenerationSettings
	{
		int minLeafSize = 10;
		int maxLeafSize = 24;
		int minRoomSize = 4;
		int roomPadding = 1;
		float splitBias = 0.5f;
	};

	DungeonGenerator();
	explicit DungeonGenerator(unsigned int seed);

	std::vector<std::string> Generate(int width, int height, const GenerationSettings& settings);
	std::vector<std::string> Generate(int width, int height);
	bool SaveToFile(const std::string& filePath) const;
	std::string ToText() const;

	const std::vector<std::string>& GetGrid() const noexcept;

private:
	struct Rect
	{
		int x = 0;
		int y = 0;
		int width = 0;
		int height = 0;
	};

	struct Node
	{
		Rect region;
		int left = -1;
		int right = -1;
		int roomIndex = -1;
	};

	struct Point
	{
		int x = 0;
		int y = 0;
	};

	struct Room
	{
		Rect bounds;
		Point center;
	};

	bool SplitNode(int nodeIndex, const GenerationSettings& settings);
	void BuildRoomsFromLeaves(int nodeIndex, const GenerationSettings& settings);
	void CarveRoom(const Room& room);
	void ConnectRooms();
	std::vector<std::pair<int, int>> BuildMinimumSpanningTree() const;
	std::vector<Point> FindCorridorPathAStar(const Point& start, const Point& end) const;
	void CarveCorridor(const std::vector<Point>& path);

	bool IsInBounds(int x, int y) const noexcept;
	bool IsBoundaryExposed() const;
	void PadBoundaryWithWalls();

private:
	unsigned int seed;
	std::vector<std::string> grid;
	std::vector<Node> bspNodes;
	std::vector<Room> rooms;
};
