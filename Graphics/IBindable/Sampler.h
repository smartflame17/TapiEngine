#pragma once
#include "IBindable.h"

class Sampler : public IBindable
{
public:
	Sampler(Graphics& gfx);
	void Bind(Graphics& gfx) noexcept override;
protected:
	Microsoft::WRL::ComPtr<ID3D11SamplerState> pSampler;
};