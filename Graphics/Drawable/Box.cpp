#include "Box.h"
#include "../PhongMaterial.h"

Box::Box(Graphics& gfx,
	std::mt19937& rng,
	std::uniform_real_distribution<float>& adist,
	std::uniform_real_distribution<float>& ddist,
	std::uniform_real_distribution<float>& odist,
	std::uniform_real_distribution<float>& rdist,
	std::uniform_real_distribution<float>& bdist)
	:
	r(rdist(rng)),
	droll(ddist(rng)),
	dpitch(ddist(rng)),
	dyaw(ddist(rng)),
	dphi(odist(rng)),
	dtheta(odist(rng)),
	dchi(odist(rng)),
	chi(adist(rng)),
	theta(adist(rng)),
	phi(adist(rng))
{
	namespace dx = DirectX;
	if (!IsStaticInitialized())	// we initialize static binds for the same type of object only once
	{
		using Type = Dvtx::VertexLayout::ElementType;
		auto model = Cube::MakeIndependent(Dvtx::VertexLayout{}
			.Append(Type::Position3D)
			.Append(Type::Normal));
		model.SetNormalsIndependentFlat();

		AddStaticBind(std::make_unique<Bind::VertexBuffer>(gfx, model.vertices));		// add vertex data

		// add shader data
		auto pvs = std::make_unique<VertexShader>(gfx, L"PhongVS.cso");
		auto pvsbc = pvs->GetBytecode();	// get vertex shader bytecode for input layout later
		AddStaticBind(std::move(pvs));

		AddStaticBind(std::make_unique<PixelShader>(gfx, L"PhongPS.cso"));

		AddStaticIndexBuffer(std::make_unique<IndexBuffer>(gfx, model.indices));	// add index buffer

		AddStaticBind(std::make_unique<InputLayout>(gfx, model.vertices.GetLayout().GetD3DLayout(), pvsbc));

		AddStaticBind(std::make_unique<Topology>(gfx, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST));
	}
	else {
		SetIndexFromStatic();	// If this object doesn't have to do static binding (not the first new object), get Index Buffer pointer
	}

	AddBind(std::make_unique<TransformCbuf>(gfx, *this));	// the transformation constant buffer is non-static (different per object)
	AddBind(std::make_unique<ShadowTransformCbuf>(gfx, *this));

	std::uniform_real_distribution<float> colorDist(0.2f, 1.0f);
	std::uniform_real_distribution<float> specularIntensityDist(0.2f, 1.0f);
	std::uniform_real_distribution<float> specularPowerDist(8.0f, 64.0f);
	const PhongMaterial material =
	{
		{ colorDist(rng), colorDist(rng), colorDist(rng) },
		specularIntensityDist(rng),
		specularPowerDist(rng),
		{ 1.0f, 1.0f, 1.0f }
	};
	AddBind(std::make_unique<PixelConstantBuffer<PhongMaterial>>(gfx, material, 0u));

	// give random deformation per instance
	dx::XMStoreFloat3x3(&mt, dx::XMMatrixScaling(1.0f, 1.0f, bdist(rng)));
}

void Box::Update(float dt) noexcept	// update angle per deltatime
{
	roll += droll * dt;
	pitch += dpitch * dt;
	yaw += dyaw * dt;
	theta += dtheta * dt;
	phi += dphi * dt;
	chi += dchi * dt;
}

DirectX::XMMATRIX Box::GetTransformXM() const noexcept
{
	return
		DirectX::XMLoadFloat3x3(&mt) *
		DirectX::XMMatrixRotationRollPitchYaw(pitch, yaw, roll) *
		DirectX::XMMatrixTranslation(r, 0.0f, 0.0f) *
		DirectX::XMMatrixRotationRollPitchYaw(theta, phi, chi) *
		GetAppliedTransformXM();
}
