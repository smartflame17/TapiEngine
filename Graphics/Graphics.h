#pragma once
#include "../SmflmWin.h"
#include "../ErrorHandling/SmflmException.h"
#include "SimpleMath.h"
#include "SpriteBatch.h"
#include "SpriteFont.h"
#include <wrl.h>
#include <DirectXMath.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <vector>
#include <memory>
#include <random>

// Wrapper class for d3d 11 stuff
class Graphics
{
	friend class IBindable;
public:
	class HrException : public SmflmException
	{
	public:
		HrException(int line, const char* file, HRESULT hr) noexcept;		// includes windows error code HRESULT
		const char* what() const noexcept override;
		virtual const char* GetType() const noexcept;
		static std::string TranslateErrorCode(HRESULT hr) noexcept;		// Translates HRESULT into error string
		HRESULT GetErrorCode() const noexcept;
		std::string GetErrorString() const noexcept;
		std::string GetErrorDescription() const noexcept;
	private:
		HRESULT hr;
	};
	class DeviceRemovedException : public HrException
	{
		using HrException::HrException;
	public:
		const char* GetType() const noexcept override;
	};

	Graphics(HWND hWnd, int width, int height);

	// Singleton stuff (no copy or move)
	Graphics(const Graphics&) = delete;
	Graphics& operator=(const Graphics&) = delete;

	~Graphics();

	void Endframe();	// flips front-back buffer
	void BeginFrame(float r, float g, float b) noexcept;		// does beginning of frame stuff (clears buffer, imgui stuff etc)
	void DrawIndexed(UINT count) noexcept(!IS_DEBUG);
	void SetProjection(DirectX::FXMMATRIX proj) noexcept;		// Sets final projection matrix after all calculation for a single frame is done
	DirectX::XMMATRIX GetProjection() const noexcept;
	void DrawTest(float angle, float x, float y, float z);

	void EnableImGui() noexcept;
	void DisableImGui() noexcept;
	bool IsImGuiEnabled() const noexcept;

	ID3D11DepthStencilState* GetDepthStencilState();

	// Overload new and delete for 16-byte alignment
	void* operator new(size_t i)
	{
		return _aligned_malloc(i, 16);
	}

	void operator delete(void* p)
	{
		_aligned_free(p);
	}
private:
	bool isImGuiEnabled = true;
	int width;
	int height;

	DirectX::XMMATRIX projection = DirectX::XMMATRIX();								// projection matrix

	Microsoft::WRL::ComPtr<ID3D11Device> pDevice = nullptr;
	Microsoft::WRL::ComPtr<IDXGISwapChain> pSwap = nullptr;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> pContext = nullptr;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> pTarget = nullptr;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> pDSV = nullptr;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> pDSState = nullptr;
public:
	std::unique_ptr<DirectX::SpriteBatch> pSpriteBatch;
	std::unique_ptr<DirectX::SpriteFont> pSpriteFont;
};