#pragma once
#include "IBindable.h"
#include "../../ErrorHandling/GraphicsExceptionMacros.h"
#include <type_traits>

template<typename C>
class ConstantBuffer : public IBindable
{
public:
	static constexpr UINT GetAlignedByteWidth() noexcept
	{
		return static_cast<UINT>((sizeof(C) + 15u) & ~15u);
	}

	void Update(Graphics& gfx, const C& consts)		// instead of creating and destroying cBuffers per frame, we map and memcpy into that address
	{
		HRESULT hr;
		D3D11_MAPPED_SUBRESOURCE msr;
		GFX_THROW_FAILED(GetContext(gfx)->Map(
			pConstantBuffer.Get(), 0u,
			D3D11_MAP_WRITE_DISCARD, 0u,
			&msr
		));
		memcpy(msr.pData, &consts, sizeof(consts));
		GetContext(gfx)->Unmap(pConstantBuffer.Get(), 0u);
	}
	ConstantBuffer(Graphics& gfx, const C& consts, UINT slot = 0u)		// init with data
		:
		slot(slot)
	{
		static_assert(std::is_standard_layout_v<C>, "Constant buffer payloads must be standard layout.");
		static_assert(sizeof(C) <= D3D11_REQ_CONSTANT_BUFFER_ELEMENT_COUNT * 16u, "Constant buffer payload exceeds D3D11 size limit.");

		HRESULT hr;
		D3D11_BUFFER_DESC cbd = {};
		cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		cbd.Usage = D3D11_USAGE_DYNAMIC;				// Dynamic so we can update it each frame
		cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;	// CPU needs to write (update) the value
		cbd.MiscFlags = 0u;
		cbd.ByteWidth = GetAlignedByteWidth();
		cbd.StructureByteStride = 0u;

		D3D11_SUBRESOURCE_DATA csd = {};
		csd.pSysMem = &consts;
		GFX_THROW_FAILED(GetDevice(gfx)->CreateBuffer(&cbd, &csd, &pConstantBuffer));
	}
	ConstantBuffer(Graphics& gfx, UINT slot = 0u)
		:
		slot(slot)
	{
		static_assert(std::is_standard_layout_v<C>, "Constant buffer payloads must be standard layout.");
		static_assert(sizeof(C) <= D3D11_REQ_CONSTANT_BUFFER_ELEMENT_COUNT * 16u, "Constant buffer payload exceeds D3D11 size limit.");

		HRESULT hr;
		D3D11_BUFFER_DESC cbd = {};
		cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		cbd.Usage = D3D11_USAGE_DYNAMIC;
		cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		cbd.MiscFlags = 0u;
		cbd.ByteWidth = GetAlignedByteWidth();
		cbd.StructureByteStride = 0u;
		GFX_THROW_FAILED(GetDevice(gfx)->CreateBuffer(&cbd, nullptr, &pConstantBuffer));
	}
protected:
	Microsoft::WRL::ComPtr<ID3D11Buffer> pConstantBuffer;
	UINT slot;
};

template<typename C>
class VertexConstantBuffer : public ConstantBuffer<C>
{
	using ConstantBuffer<C>::pConstantBuffer;
	using ConstantBuffer<C>::slot;
	using IBindable::GetContext;		// template inheritance fix (declare explicitly)
public:
	using ConstantBuffer<C>::ConstantBuffer;
	void Bind(Graphics& gfx) noexcept override
	{
		GetContext(gfx)->VSSetConstantBuffers(slot, 1u, pConstantBuffer.GetAddressOf());
	}
};

template<typename C>
class PixelConstantBuffer : public ConstantBuffer<C>
{
	using ConstantBuffer<C>::pConstantBuffer;
	using ConstantBuffer<C>::slot;
	using IBindable::GetContext;
public:
	using ConstantBuffer<C>::ConstantBuffer;
	void Bind(Graphics& gfx) noexcept override
	{
		GetContext(gfx)->PSSetConstantBuffers(slot, 1u, pConstantBuffer.GetAddressOf());
	}
};
