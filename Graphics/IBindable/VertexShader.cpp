#include "VertexShader.h"
#include "../../ErrorHandling/GraphicsExceptionMacros.h"

VertexShader::VertexShader(Graphics& gfx, const std::wstring& path)
{
	HRESULT hr;
	GFX_THROW_FAILED( D3DReadFileToBlob(path.c_str(), &pBytecodeBlob) );
	GFX_THROW_FAILED(GetDevice(gfx)->CreateVertexShader(
		pBytecodeBlob->GetBufferPointer(),
		pBytecodeBlob->GetBufferSize(),
		nullptr,
		&pVertexShader
	));
}

void VertexShader::Bind(Graphics& gfx) noexcept
{
	BindShader(gfx);
}

void VertexShader::BindShader(Graphics& gfx) const noexcept
{
	GetContext(gfx)->VSSetShader(pVertexShader.Get(), nullptr, 0u);
}

ID3DBlob* VertexShader::GetBytecode() const noexcept
{
	return pBytecodeBlob.Get();
}

