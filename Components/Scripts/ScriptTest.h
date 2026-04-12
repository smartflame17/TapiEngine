#pragma once
#include "../CustomBehaviour.h"
#include <string>

class ScriptTest : public CustomBehaviour
{
public:
	ScriptTest(GameObject* owner) : CustomBehaviour(owner) {}
	~ScriptTest() override = default;
	void Start() override;
	void Update(float dt) override;
	void ExposeVariables() override
	{
		ExposeInt("Test Int", &testInt);
		ExposeFloat("Test Float", &testFloat);
		ExposeString("Exposed String", &exposedString);
	}

	int testInt = 0;
	float testFloat = 0.0f;
	std::string exposedString = "Hello World!";
};