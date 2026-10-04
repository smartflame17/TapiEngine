#pragma once
#include "../CustomBehaviour.h"
#include "../../Audio/Audio.h"
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
		ExposeVector3("Test Vector", testVector);
		ExposeColor("Test Color", testColor);
		ExposeBool("Test Bool", &testBool);
	}

	int testInt = 0;
	float testFloat = 0.0f;
	std::string exposedString = "Hello World!";
	float testVector[3] = { 0, 0, 0 };
	float testColor[3] = { 1, 1, 1 };
	bool testBool = true;
};
