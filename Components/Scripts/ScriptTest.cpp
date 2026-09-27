#include "ScriptTest.h"
#include <iostream>

void ScriptTest::Start()
{
	std::cout << "ScriptTest Start called" << std::endl;
	auto& audio = Audio::GetInstance();

	//audio.PlayOneShot("Audio/Sounds/Attack2.wav", 1.0f, 0.0f, 0.0f);
	//audio.Play("Audio/Sounds/lotus_waters.wav", false, 1.0f, 0.0f, 0.0f);
}

void ScriptTest::Update(float dt)
{
	std::cout << "ScriptTest Update called with dt: " << dt << std::endl;
}

REGISTER_SCRIPT(ScriptTest)