#include "InputLayout.h"
#include "../../ErrorHandling/GraphicsExceptionMacros.h"

InputLayout::InputLayout(Graphics& gfx,
	const std::vector<D3D11_INPUT_ELEMENT_DESC>& layout,
	ID3DBlob* pVertexShaderBytecode)
	:
	layoutDesc(layout)
{
	HRESULT hr;
	GFX_THROW_FAILED(GetDevice(gfx)->CreateInputLayout(
		layoutDesc.data(), (UINT)layoutDesc.size(),
		pVertexShaderBytecode->GetBufferPointer(),
		pVertexShaderBytecode->GetBufferSize(),
		&pInputLayout
	));
}

void InputLayout::Bind(Graphics& gfx) noexcept
{
	GetContext(gfx)->IASetInputLayout(pInputLayout.Get());
}

void InputLayout::BindForShader(Graphics& gfx, ID3DBlob* pVertexShaderBytecode) noexcept(!IS_DEBUG)
{
	if (pVertexShaderBytecode == nullptr)
	{
		Bind(gfx);
		return;
	}

	if (pAlternateBytecode != pVertexShaderBytecode || pAlternateInputLayout == nullptr)
	{
		HRESULT hr;
		pAlternateInputLayout.Reset();
		GFX_THROW_FAILED(GetDevice(gfx)->CreateInputLayout(
			layoutDesc.data(), (UINT)layoutDesc.size(),
			pVertexShaderBytecode->GetBufferPointer(),
			pVertexShaderBytecode->GetBufferSize(),
			&pAlternateInputLayout
		));
		pAlternateBytecode = pVertexShaderBytecode;
	}

	GetContext(gfx)->IASetInputLayout(pAlternateInputLayout.Get());
}

