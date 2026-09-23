#pragma once
#include "Component.h"
#include "../Audio/Audio.h"

class AudioClip final : public Component
{
public:
	static constexpr ComponentType StaticType = ComponentType::AudioClip;
	AudioClip() noexcept;
	~AudioClip() override;

	AudioClip(const AudioClip&) = delete;
	AudioClip& operator=(const AudioClip&) = delete;

private:
	friend class GameObject;
	AudioHandle handle = InvalidAudioHandle;
};