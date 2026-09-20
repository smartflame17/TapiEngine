#pragma once
#include <Audio.h>

class App;

class Audio
{
public:

	~Audio() noexcept;
	Audio(const Audio&) = delete;
	Audio& operator=(const Audio&) = delete;
	Audio& operator=(Audio&&) = delete;
	Audio(Audio&&) = delete;

	static Audio& GetInstance();

private:
	friend class App;

	static Audio* instance;
	Audio();
};