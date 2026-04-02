#include "Mesh.h"
#include "../../imgui/imgui.h"
#include <utility>

Mesh::Mesh(Graphics& gfx,
	std::vector<std::unique_ptr<IBindable>> bindPtrs,
	PhongMaterial material,
	PixelConstantBuffer<PhongMaterial>* pMaterialCbuf,
	Texture* pBaseColorTexture,
	Texture* pNormalTexture,
	Sampler* pSampler,
	std::string baseColorTexturePath,
	std::string normalMapPath,
	bool supportsTextureMapping,
	bool normalMapEnabled)
	:
	material(material),
	pMaterialCbuf(pMaterialCbuf),
	pBaseColorTexture(pBaseColorTexture),
	pNormalTexture(pNormalTexture),
	pSampler(pSampler),
	baseColorTexturePath(std::move(baseColorTexturePath)),
	normalMapPath(std::move(normalMapPath)),
	supportsTextureMapping(supportsTextureMapping),
	normalMapEnabled(normalMapEnabled)
{
	if (!IsStaticInitialized())
	{
		AddStaticBind(std::make_unique<Topology>(gfx, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST));
	}

	for (auto& bind : bindPtrs)
	{
		if (auto pIndexBuffer = dynamic_cast<IndexBuffer*>(bind.get()))
		{
			AddIndexBuffer(std::unique_ptr<IndexBuffer>{ pIndexBuffer });
			bind.release();
		}
		else
		{
			AddBind(std::move(bind));
		}
	}

	AddBind(std::make_unique<TransformCbuf>(gfx, *this));
	AddBind(std::make_unique<ShadowTransformCbuf>(gfx, *this));
	RefreshMaterialState();
	UpdateBaseColorStatus(pBaseColorTexture != nullptr && !pBaseColorTexture->IsUsingFallback() && !baseColorTexturePath.empty());
	UpdateNormalMapStatus(pNormalTexture != nullptr && !pNormalTexture->IsUsingFallback() && !normalMapPath.empty());
}

void Mesh::Draw(Graphics& gfx, DirectX::FXMMATRIX accumulatedTransform) const noexcept(!IS_DEBUG)
{
	DirectX::XMStoreFloat4x4(&transform, accumulatedTransform);
	if (pMaterialCbuf != nullptr)
	{
		pMaterialCbuf->Update(gfx, material);
		pMaterialCbuf->Bind(gfx);
	}
	if (pBaseColorTexture != nullptr)
	{
		pBaseColorTexture->Bind(gfx);
	}
	if (pNormalTexture != nullptr)
	{
		pNormalTexture->Bind(gfx);
	}
	if (pSampler != nullptr)
	{
		pSampler->Bind(gfx);
	}
	Drawable::Draw(gfx);
}

void Mesh::DrawShadow(Graphics& gfx, DirectX::FXMMATRIX accumulatedTransform, ID3DBlob* pShadowVertexShaderBytecode) const noexcept(!IS_DEBUG)
{
	DirectX::XMStoreFloat4x4(&transform, accumulatedTransform);
	Drawable::DrawShadow(gfx, pShadowVertexShaderBytecode);
}

DirectX::XMMATRIX Mesh::GetTransformXM() const noexcept
{
	return DirectX::XMLoadFloat4x4(&transform);
}

void Mesh::DrawInspector(Graphics& gfx, const char* label) noexcept
{
	const bool open = label == nullptr || ImGui::TreeNode(label);
	if (!open)
	{
		return;
	}

	ImGui::ColorEdit3("Base Color", &material.color.x);
	ImGui::ColorEdit3("Specular Color", &material.specularColor.x);
	ImGui::SliderFloat("Specular Intensity", &material.specularIntensity, 0.0f, 2.0f, "%.2f");
	ImGui::SliderFloat("Specular Power", &material.specularPower, 1.0f, 128.0f, "%.1f");

	if (supportsTextureMapping)
	{
		static constexpr const char* samplerLabels[] =
		{
			"Linear Wrap",
			"Point Wrap",
			"Linear Clamp",
			"Point Clamp",
			"Anisotropic Wrap",
			"Anisotropic Clamp"
		};

		ImGui::Separator();
		int selectedSampler = static_cast<int>(pSampler != nullptr ? pSampler->GetType() : Sampler::Type::LinearWrap);
		if (ImGui::Combo("Sampler", &selectedSampler, samplerLabels, IM_ARRAYSIZE(samplerLabels)) && pSampler != nullptr)
		{
			pSampler->SetType(gfx, static_cast<Sampler::Type>(selectedSampler));
		}

		char baseColorPathBuffer[260] = {};
		baseColorTexturePath.copy(baseColorPathBuffer, sizeof(baseColorPathBuffer) - 1u);
		if (ImGui::InputText("Texture Path", baseColorPathBuffer, IM_ARRAYSIZE(baseColorPathBuffer)))
		{
			baseColorTexturePath = baseColorPathBuffer;
		}
		if (ImGui::Button("Apply Texture"))
		{
			ApplyBaseColorTexturePath(gfx);
		}
		ImGui::TextWrapped("%s", baseColorTextureStatus.c_str());

		bool enableNormalMap = normalMapEnabled;
		if (ImGui::Checkbox("Use Normal Map", &enableNormalMap))
		{
			normalMapEnabled = enableNormalMap;
			RefreshMaterialState();
		}

		char normalMapPathBuffer[260] = {};
		normalMapPath.copy(normalMapPathBuffer, sizeof(normalMapPathBuffer) - 1u);
		if (ImGui::InputText("Normal Map Path", normalMapPathBuffer, IM_ARRAYSIZE(normalMapPathBuffer)))
		{
			normalMapPath = normalMapPathBuffer;
		}
		if (ImGui::Button("Apply Normal Map"))
		{
			ApplyNormalMapPath(gfx);
		}
		ImGui::TextWrapped("%s", normalMapStatus.c_str());
	}

	if (label != nullptr)
	{
		ImGui::TreePop();
	}
}

void Mesh::ApplyBaseColorTexturePath(Graphics& gfx) noexcept
{
	if (pBaseColorTexture == nullptr)
	{
		return;
	}

	const bool loadedFromFile = pBaseColorTexture->SetPath(gfx, std::filesystem::path(baseColorTexturePath));
	UpdateBaseColorStatus(loadedFromFile);
}

void Mesh::ApplyNormalMapPath(Graphics& gfx) noexcept
{
	if (pNormalTexture == nullptr)
	{
		return;
	}

	const bool loadedFromFile = pNormalTexture->SetPath(gfx, std::filesystem::path(normalMapPath));
	UpdateNormalMapStatus(loadedFromFile);
	RefreshMaterialState();
}

void Mesh::RefreshMaterialState() noexcept
{
	material.useNormalMap = normalMapEnabled && pNormalTexture != nullptr && !pNormalTexture->IsUsingFallback() && !normalMapPath.empty() ? 1u : 0u;
}

void Mesh::UpdateBaseColorStatus(bool loadedFromFile) noexcept
{
	if (loadedFromFile)
	{
		baseColorTextureStatus = "Loaded texture: " + baseColorTexturePath;
		return;
	}

	if (baseColorTexturePath.empty())
	{
		baseColorTextureStatus = "Using fallback checkerboard texture (empty path).";
		return;
	}

	baseColorTextureStatus = "Using fallback checkerboard texture. Failed to load: " + baseColorTexturePath;
}

void Mesh::UpdateNormalMapStatus(bool loadedFromFile) noexcept
{
	if (loadedFromFile)
	{
		normalMapStatus = normalMapEnabled
			? "Loaded normal map: " + normalMapPath
			: "Normal map loaded but disabled: " + normalMapPath;
		return;
	}

	if (normalMapPath.empty())
	{
		normalMapStatus = "Using interpolated normals (no normal map path).";
		return;
	}

	normalMapStatus = "Using interpolated normals. Failed to load: " + normalMapPath;
}
