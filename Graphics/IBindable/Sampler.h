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
		AnisotropicClamp,
		ComparisonLinearClamp
	};

	Sampler(Graphics& gfx, Type type = Type::LinearWrap, UINT slot = 0u);
	void Bind(Graphics& gfx) noexcept override;
	void SetType(Graphics& gfx, Type newType);
	Type GetType() const noexcept;
protected:
	void CreateSampler(Graphics& gfx, Type type);

	Microsoft::WRL::ComPtr<ID3D11SamplerState> pSampler;
	Type type = Type::LinearWrap;
	UINT slot = 0u;
};
