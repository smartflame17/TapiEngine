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

class DebugWireframeRenderer;

// Wrapper class for d3d 11 stuff
class Graphics
{
	friend class IBindable;
	friend class DebugWireframeRenderer;
public:
	struct WireframeDebugSettings
	{
		bool enabled = false;
		DirectX::XMFLOAT3 color = { 0.0f, 1.0f, 0.0f };
	};

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
	int GetWidth() const noexcept;
	int GetHeight() const noexcept;
	float GetViewportWidth() const noexcept;
	float GetViewportHeight() const noexcept;
	float GetViewportTopLeftX() const noexcept;
	float GetViewportTopLeftY() const noexcept;

	void SetCamera(DirectX::FXMMATRIX cam) noexcept;			// Sets camera matrix for the frame
	DirectX::XMMATRIX GetCamera() const noexcept;

	void DrawTest(float angle, float x, float y, float z);

	void EnableImGui() noexcept;
	void DisableImGui() noexcept;
	bool IsImGuiEnabled() const noexcept;
	void BindMainRenderTarget() noexcept;
	void ClearMainRenderTarget(float r, float g, float b, bool clearDepth = true, bool clearStencil = true) noexcept;
	void RestoreDefaultStates() noexcept;
	void DrawWireframeBoundingBox(const DirectX::BoundingBox& bounds) noexcept(!IS_DEBUG);
	void DrawWireframeBoundingBoxes(const std::vector<DirectX::BoundingBox>& bounds) noexcept(!IS_DEBUG);

	ID3D11DepthStencilState* GetDepthStencilState();
	WireframeDebugSettings& GetWireframeDebugSettings() noexcept;
	const WireframeDebugSettings& GetWireframeDebugSettings() const noexcept;

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

	float viewportWidth;
	float viewportHeight;
	float viewportTopLeftX;
	float viewportTopLeftY;

	DirectX::XMMATRIX projection = DirectX::XMMATRIX();								// projection matrix
	DirectX::XMMATRIX camera = DirectX::XMMATRIX();									// camera matrix

	Microsoft::WRL::ComPtr<ID3D11Device> pDevice = nullptr;
	Microsoft::WRL::ComPtr<IDXGISwapChain> pSwap = nullptr;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> pContext = nullptr;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> pTarget = nullptr;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> pDSV = nullptr;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> pDSState = nullptr;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> pRSState = nullptr;
	Microsoft::WRL::ComPtr<ID3D11BlendState> pBlendState = nullptr;
	std::unique_ptr<DebugWireframeRenderer> pDebugWireframeRenderer;
	WireframeDebugSettings wireframeDebugSettings;

public:
	std::unique_ptr<DirectX::SpriteBatch> pSpriteBatch;
	std::unique_ptr<DirectX::SpriteFont> pSpriteFont;
};
