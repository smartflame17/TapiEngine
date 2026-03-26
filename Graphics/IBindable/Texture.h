#pragma once
#include "IBindable.h"
#include <filesystem>

class Texture : public IBindable
{
public:
	enum class FallbackKind
	{
		Checkerboard,
		NeutralNormal
	};

	Texture(Graphics& gfx, const std::wstring& path, UINT slot = 0u, FallbackKind fallbackKind = FallbackKind::Checkerboard);
	Texture(Graphics& gfx, UINT slot = 0u, FallbackKind fallbackKind = FallbackKind::Checkerboard);
	void Bind(Graphics& gfx) noexcept override;
	bool SetPath(Graphics& gfx, const std::filesystem::path& path) noexcept;
	const std::filesystem::path& GetRequestedPath() const noexcept;
	bool IsUsingFallback() const noexcept;
protected:
	void LoadFromFile(Graphics& gfx, const std::wstring& path);
	void LoadFallback(Graphics& gfx);

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> pTextureView;
	UINT slot = 0u;
	FallbackKind fallbackKind = FallbackKind::Checkerboard;
	std::filesystem::path requestedPath;
	bool usingFallback = false;
};
