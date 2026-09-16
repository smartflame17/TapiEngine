#pragma once

#include <algorithm>
#include <cassert>

// Scheduling only: the caller consumes ticks before rendering and owns play/pause state.
class FixedStepClock
{
public:
	static constexpr double MaxAccumulatedTime = 0.25;

	explicit FixedStepClock(double timeStep) noexcept : timeStep(timeStep)
	{
		assert(timeStep > 0.0 && timeStep <= MaxAccumulatedTime);
	}

	void Advance(double elapsed) noexcept
	{
		if (elapsed > 0.0)
		{
			accumulator = (std::min)(accumulator + elapsed, MaxAccumulatedTime);
		}
	}

	bool ConsumeStep() noexcept
	{
		// Allow roundoff at exact tick boundaries (e.g. 144 render frames per second).
		constexpr double relativeTolerance = 1.0e-10;
		if (accumulator + timeStep * relativeTolerance < timeStep)
		{
			return false;
		}
		accumulator = (std::max)(0.0, accumulator - timeStep);
		return true;
	}

	void Reset() noexcept { accumulator = 0.0; }
	// Call after consuming all available ticks; the result lies in [0, 1).
	double GetAlpha() const noexcept { return accumulator / timeStep; }

private:
	const double timeStep;
	double accumulator = 0.0;
};
