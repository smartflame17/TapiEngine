#include "ScriptTest.h"
#include <iostream>

void ScriptTest::Start()
{
	std::cout << "ScriptTest Start called" << std::endl;
}

void ScriptTest::Update(float dt)
{
	std::cout << "ScriptTest Update called with dt: " << dt << std::endl;
}

REGISTER_SCRIPT(ScriptTest)