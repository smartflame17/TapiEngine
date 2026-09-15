#pragma once
#include "IBindable/VertexShader.h"

struct ShadowDrawContext
{
	const VertexShader& rigid;
	const VertexShader& skinned;
};
