#include "../SmflmWin.h"
#include "Mouse.h"


std::pair<int, int> Mouse::GetPos() const noexcept
{
	return { x, y };
}

std::optional<Mouse::RawDelta> Mouse::ReadRawDelta() noexcept
{
	if (rawDeltaBuffer.empty()) {
		return std::nullopt;
	}
	const RawDelta delta = rawDeltaBuffer.front();
	rawDeltaBuffer.pop();
	return delta;
}

int Mouse::GetPosX() const noexcept
{
	return x;
}

int Mouse::GetPosY() const noexcept
{
	return y;
}

bool Mouse::IsLeftPressed() const noexcept
{
	return isLeftPressed;
}

bool Mouse::IsRightPressed() const noexcept
{
	return isRightPressed;
}

bool Mouse::IsMiddlePressed() const noexcept
{
	return isMiddlePressed;
}

bool Mouse::IsInWindow() const noexcept
{
	return isInWindow;
}

Mouse::Event Mouse::Read() noexcept
{
	if (buffer.size() > 0) {
		Mouse::Event e = buffer.front();
		buffer.pop();
		return e;
	}
	else return Mouse::Event();
}

void Mouse::Flush() noexcept
{
	buffer = std::queue<Event>();
}


// Implementation of windows-side handling functions
void Mouse::TrimBuffer() noexcept
{
	while (buffer.size() > bufferSize)
		buffer.pop();
}

void Mouse::TrimRawInputBuffer() noexcept
{
	while (rawDeltaBuffer.size() > bufferSize)
		rawDeltaBuffer.pop();
}

void Mouse::OnMouseMove(int new_x, int new_y) noexcept
{
	x = new_x;
	y = new_y;

	buffer.push(Mouse::Event(Mouse::Event::Type::Move, *this));
	TrimBuffer();
}

void Mouse::OnMouseLeave() noexcept
{
	isInWindow = false;
	buffer.push(Mouse::Event(Mouse::Event::Type::Leave, *this));
	TrimBuffer();
}

void Mouse::OnMouseEnter() noexcept
{
	isInWindow = true;
	buffer.push(Mouse::Event(Mouse::Event::Type::Enter, *this));
	TrimBuffer();
}

void Mouse::OnRawDelta(int dx, int dy) noexcept
{
	rawDeltaBuffer.push({ dx, dy });
	TrimRawInputBuffer();
}

void Mouse::OnLeftPressed(int x, int y) noexcept
{
	isLeftPressed = true;
	buffer.push(Mouse::Event(Mouse::Event::Type::LPressed, *this));
	TrimBuffer();
}

void Mouse::OnRightPressed(int x, int y) noexcept
{
	isRightPressed = true;
	buffer.push(Mouse::Event(Mouse::Event::Type::RPressed, *this));
	TrimBuffer();
}

void Mouse::OnMiddlePressed(int x, int y) noexcept
{
	isMiddlePressed = true;
	buffer.push(Mouse::Event(Mouse::Event::Type::MPressed, *this));
	TrimBuffer();
}

void Mouse::OnLeftReleased(int x, int y) noexcept
{
	isLeftPressed = false;
	buffer.push(Mouse::Event(Mouse::Event::Type::LReleased, *this));
	TrimBuffer();
}

void Mouse::OnRightReleased(int x, int y) noexcept
{
	isRightPressed = false;
	buffer.push(Mouse::Event(Mouse::Event::Type::RReleased, *this));
	TrimBuffer();
}

void Mouse::OnMiddleReleased(int x, int y) noexcept
{
	isMiddlePressed = false;
	buffer.push(Mouse::Event(Mouse::Event::Type::MReleased, *this));
	TrimBuffer();
}

void Mouse::OnWheelUp(int x, int y) noexcept
{
	buffer.push(Mouse::Event(Mouse::Event::Type::WheelUp, *this));
	TrimBuffer();
}

void Mouse::OnWheelDown(int x, int y) noexcept
{
	buffer.push(Mouse::Event(Mouse::Event::Type::WheelDown, *this));
	TrimBuffer();
}

void Mouse::OnWheelDelta(int x, int y, int delta) noexcept
{
	wheelDeltaCarry += delta;
	// Accumulate up to 120 (or -120) and generate mouse wheel event (For Compatibility against free-scroll wheels with more precision)
	while (wheelDeltaCarry > WHEEL_DELTA)
	{
		wheelDeltaCarry -= WHEEL_DELTA;
		OnWheelUp(x, y);
	}
	while (wheelDeltaCarry <= -WHEEL_DELTA)
	{
		wheelDeltaCarry += WHEEL_DELTA;
		OnWheelDown(x, y);
	}
}

void Mouse::EnableRaw() noexcept
{
	rawEnabled = true;
}

void Mouse::DisableRaw() noexcept
{
	rawEnabled = false;
}

bool Mouse::RawEnabled() const noexcept
{
	return rawEnabled;
}