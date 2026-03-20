#pragma once
#include "IBindable.h"

class Sampler : public IBindable
{
public:
	enum class Type
	{
		LinearWrap,
		PointWrap,
		LinearClamp,
		PointClamp,
		AnisotropicWrap,
		AnisotropicClamp
	};

	Sampler(Graphics& gfx, Type type = Type::LinearWrap);
	void Bind(Graphics& gfx) noexcept override;
	void SetType(Graphics& gfx, Type newType);
	Type GetType() const noexcept;
protected:
	void CreateSampler(Graphics& gfx, Type type);

	Microsoft::WRL::ComPtr<ID3D11SamplerState> pSampler;
	Type type = Type::LinearWrap;
};
