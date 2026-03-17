 #pragma once
#include <queue>
#include <optional>

// Very similar implementation to the Keyboard class
class Mouse
{
	friend class Window;
public:
	struct RawDelta
	{
		int x;
		int y;
	};
	class Event
	{
	public:
		enum class Type
		{
			// M1, M2 button
			LPressed,
			LReleased,

			RPressed,
			RReleased,
			// M3 button (wheel click)
			MPressed,
			MReleased,

			WheelUp,
			WheelDown,

			// For mouse capturing outside of the window
			Enter,
			Leave,

			Move,
			Invalid
		};
	private:
		Type type;
		bool isLeftPressed;
		bool isRightPressed;
		bool isMiddlePressed;
		int x;
		int y;
	public:
		Event() noexcept:
			type(Type::Invalid),
			isLeftPressed(false),
			isRightPressed(false),
			isMiddlePressed(false),
			x(0),
			y(0)
		{}
		Event(Type type, const Mouse& parent) noexcept :
			type(type),
			isLeftPressed(parent.isLeftPressed),
			isRightPressed(parent.isRightPressed),
			isMiddlePressed(parent.isMiddlePressed),
			x(parent.x),
			y(parent.y)
		{}
		bool IsValid() const noexcept
		{
			return type != Type::Invalid;
		}
		Type GetType() const noexcept
		{
			return type;
		}
		std::pair<int, int> GetPos() const noexcept		// returns pair of (x, y) position of mouse
		{
			return { x, y };
		}
		int GetPosX() const noexcept
		{
			return x;
		}
		int GetPosY() const noexcept
		{
			return y;
		}
		bool IsLeftPressed() const noexcept
		{
			return isLeftPressed;
		}
		bool IsRightPressed() const noexcept
		{
			return isRightPressed;
		}
		bool IsMiddlePressed() const noexcept
		{
			return isMiddlePressed;
		}
	};
public:
	Mouse() = default;
	Mouse(const Mouse&) = delete;
	Mouse& operator=(const Mouse&) = delete;		// singleton stuff

	// mouse position related
	std::pair<int, int> GetPos() const noexcept;
	std::optional<RawDelta> ReadRawDelta() noexcept;		// for raw input, returns delta of mouse movement since last event (for better precision and no acceleration)
	int GetPosX() const noexcept;
	int GetPosY() const noexcept;
	bool IsInWindow() const noexcept;

	// button press
	bool IsLeftPressed() const noexcept;
	bool IsRightPressed() const noexcept;
	bool IsMiddlePressed() const noexcept;
	Mouse::Event Read() noexcept;

	// raw input mode
	void EnableRaw() noexcept;
	void DisableRaw() noexcept;
	bool RawEnabled() const noexcept;

	// buffer related
	bool isEmpty() const noexcept
	{
		return buffer.empty();
	}
	void Flush() noexcept;

private:	// Windows-side handling (invisible to user)
	void OnMouseMove(int x, int y) noexcept;
	void OnMouseLeave() noexcept;
	void OnMouseEnter() noexcept;

	void OnLeftPressed(int x, int y) noexcept;
	void OnRightPressed(int x, int y) noexcept;
	void OnLeftReleased(int x, int y) noexcept;
	void OnRightReleased(int x, int y) noexcept;
	
	void OnMiddlePressed(int x, int y) noexcept;
	void OnMiddleReleased(int x, int y) noexcept;

	void OnWheelUp(int x, int y) noexcept;
	void OnWheelDown(int x, int y) noexcept;

	void TrimBuffer() noexcept;
	void TrimRawInputBuffer() noexcept;
	void OnWheelDelta(int x, int y, int delta) noexcept;

	// for raw input
	void OnRawDelta(int dx, int dy) noexcept;

private:
	static constexpr unsigned int bufferSize = 16u;
	int x;
	int y;
	bool isLeftPressed = false;
	bool isRightPressed = false;
	bool isMiddlePressed = false;
	bool isInWindow = false;
	bool rawEnabled = true;
	int wheelDeltaCarry = 0;
	std::queue<Event> buffer;
	std::queue<RawDelta> rawDeltaBuffer;
};