#pragma once
#include "../Components/CustomBehaviour.h"
#include <stdexcept>

class SerializationProbe : public CustomBehaviour
{
public:
	explicit SerializationProbe(GameObject* owner = nullptr) : CustomBehaviour(owner) {}
	void ExposeVariables() override
	{
		ExposeInt("integer", &integer); ExposeFloat("number", &number); ExposeString("text", &text);
		ExposeVector3("vector", vector); ExposeColor("color", color); ExposeBool("flag", &flag);
		if (duplicate) ExposeInt("integer", &integer);
		if (nullPointer) ExposeInt("null", nullptr);
	}
	void Awake() override { ++awakes; }
	void OnEnable() override { ++enables; }
	void Start() override { ++starts; }
	void Update(float) override { ++updates; }
	void OnDisable() override { ++disables; }
	static void ResetCounts() { awakes = enables = starts = updates = disables = 0; }
	inline static int awakes = 0, enables = 0, starts = 0, updates = 0, disables = 0;
	inline static bool duplicate = false, nullPointer = false;
	int integer = 17;
	float number = 1.25f;
	std::string text = "compiled default";
	float vector[3] = { 1, 2, 3 }, color[3] = { 0.2f, 0.4f, 0.6f };
	bool flag = true;
};
REGISTER_SCRIPT(SerializationProbe)

class ThrowingSerializationScript : public CustomBehaviour
{
public:
	explicit ThrowingSerializationScript(GameObject* owner = nullptr) : CustomBehaviour(owner)
	{
		SetEnabled(false);
		if (shouldThrow) throw std::runtime_error("script constructor fixture failure");
	}
	inline static bool shouldThrow = true;
};
REGISTER_SCRIPT(ThrowingSerializationScript)
