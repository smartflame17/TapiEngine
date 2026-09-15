#define NOMINMAX
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include "../Graphics/Animation/Animation.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

using Microsoft::WRL::ComPtr;
namespace dx = DirectX;
using namespace DirectX;
namespace
{
void Check(HRESULT result) { if (FAILED(result)) throw std::runtime_error("D3D11 failure: " + std::to_string(result)); }
struct Vertex
{
	dx::XMFLOAT3 position, normal;
	dx::XMFLOAT2 uv;
	dx::XMFLOAT3 tangent;
	dx::XMUINT4 indices;
	dx::XMFLOAT4 weights;
};
static_assert(sizeof(Vertex) == 76);
ComPtr<ID3DBlob> Compile(const std::string& source, const char* profile)
{
	ComPtr<ID3DBlob> code, errors;
	const auto hr = D3DCompile(source.data(), source.size(), nullptr, nullptr, nullptr, "main", profile,
		D3DCOMPILE_ENABLE_STRICTNESS, 0, &code, &errors);
	if (FAILED(hr) && errors) std::cerr << static_cast<const char*>(errors->GetBufferPointer());
	Check(hr); return code;
}
ComPtr<ID3D11Buffer> Buffer(ID3D11Device* device, UINT size, UINT flags, const void* data = nullptr)
{
	D3D11_BUFFER_DESC desc = {}; desc.ByteWidth = size; desc.Usage = D3D11_USAGE_DEFAULT; desc.BindFlags = flags;
	D3D11_SUBRESOURCE_DATA initial = {}; initial.pSysMem = data;
	ComPtr<ID3D11Buffer> result; Check(device->CreateBuffer(&desc, data ? &initial : nullptr, &result)); return result;
}
float Compare(const float* a, const float* b, std::size_t count)
{
	float error = 0;
	for (std::size_t i=0;i<count;++i)
	{
		if (!std::isfinite(a[i])) throw std::runtime_error("Nonfinite GPU output");
		error = (std::max)(error, std::abs(a[i]-b[i]));
	}
	return error;
}

void Run(ID3D11Device* device, ID3D11DeviceContext* context, const wchar_t* file, bool textured, bool shadow,
	const std::vector<Vertex>& vertices, const std::vector<dx::XMFLOAT4X4>& palette)
{
	ComPtr<ID3DBlob> bytecode; Check(D3DReadFileToBlob(file, &bytecode));
	ComPtr<ID3D11VertexShader> vs;
	Check(device->CreateVertexShader(bytecode->GetBufferPointer(), bytecode->GetBufferSize(), nullptr, &vs));
	std::vector<D3D11_INPUT_ELEMENT_DESC> layout = {{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0}};
	if (!shadow) layout.push_back({"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0});
	if (textured)
	{
		layout.push_back({"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,24,D3D11_INPUT_PER_VERTEX_DATA,0});
		layout.push_back({"TANGENT",0,DXGI_FORMAT_R32G32B32_FLOAT,0,32,D3D11_INPUT_PER_VERTEX_DATA,0});
	}
	layout.push_back({"BLENDINDICES",0,DXGI_FORMAT_R32G32B32A32_UINT,0,44,D3D11_INPUT_PER_VERTEX_DATA,0});
	layout.push_back({"BLENDWEIGHT",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,60,D3D11_INPUT_PER_VERTEX_DATA,0});
	ComPtr<ID3D11InputLayout> inputLayout;
	Check(device->CreateInputLayout(layout.data(), UINT(layout.size()), bytecode->GetBufferPointer(), bytecode->GetBufferSize(), &inputLayout));
	std::string fields = shadow ? "" : "float3 world:Position; float3 normal:NORMAL;";
	if (textured) fields += "float3 tangent:TANGENT; float2 tex:TEXCOORD;";
	fields += "float4 pos:SV_POSITION;";
	auto gsCode = Compile("struct V{"+fields+"}; [maxvertexcount(1)] void main(point V v[1],inout PointStream<V> stream){stream.Append(v[0]);}", "gs_4_0");
	std::vector<D3D11_SO_DECLARATION_ENTRY> declaration;
	if (!shadow) { declaration.push_back({0,"Position",0,0,3,0}); declaration.push_back({0,"NORMAL",0,0,3,0}); }
	if (textured) { declaration.push_back({0,"TANGENT",0,0,3,0}); declaration.push_back({0,"TEXCOORD",0,0,2,0}); }
	declaration.push_back({0,"SV_POSITION",0,0,4,0});
	UINT outputStride = shadow ? 16 : textured ? 60 : 40;
	ComPtr<ID3D11GeometryShader> gs;
	Check(device->CreateGeometryShaderWithStreamOutput(gsCode->GetBufferPointer(), gsCode->GetBufferSize(), declaration.data(),
		UINT(declaration.size()), &outputStride, 1, D3D11_SO_NO_RASTERIZED_STREAM, nullptr, &gs));
	const auto model = dx::XMMatrixScaling(1.5f,.7f,2.0f)*dx::XMMatrixRotationY(.35f)*dx::XMMatrixTranslation(4,2,-3);
	const auto viewProjection = dx::XMMatrixLookAtLH(dx::XMVectorSet(2,4,-10,1),dx::XMVectorZero(),dx::XMVectorSet(0,1,0,0))*
		dx::XMMatrixPerspectiveFovLH(.8f,1.5f,.1f,100.0f);
	dx::XMFLOAT4X4 transforms[3];
	dx::XMStoreFloat4x4(&transforms[0],dx::XMMatrixTranspose(model*viewProjection));
	dx::XMStoreFloat4x4(&transforms[1],dx::XMMatrixTranspose(model));
	dx::XMStoreFloat4x4(&transforms[2],dx::XMMatrixTranspose(Animation::NormalMatrix(model)));
	Animation::SkinningConstants constants; Animation::PrepareSkinningConstants(palette,constants);
	auto transformsBuffer=Buffer(device,sizeof(transforms),D3D11_BIND_CONSTANT_BUFFER,transforms);
	auto skinBuffer=Buffer(device,sizeof(constants),D3D11_BIND_CONSTANT_BUFFER,&constants);
	auto vertexBuffer=Buffer(device,UINT(vertices.size()*sizeof(Vertex)),D3D11_BIND_VERTEX_BUFFER,vertices.data());
	auto output=Buffer(device,UINT(vertices.size()*outputStride),D3D11_BIND_STREAM_OUTPUT);
	ID3D11Buffer* buffers[]={transformsBuffer.Get(),skinBuffer.Get()}; context->VSSetConstantBuffers(0,2,buffers);
	UINT stride=sizeof(Vertex),offset=0; auto* vb=vertexBuffer.Get(); auto* out=output.Get();
	context->IASetVertexBuffers(0,1,&vb,&stride,&offset); context->IASetInputLayout(inputLayout.Get());
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
	context->VSSetShader(vs.Get(),nullptr,0); context->GSSetShader(gs.Get(),nullptr,0); context->PSSetShader(nullptr,nullptr,0);
	context->SOSetTargets(1,&out,&offset); context->Draw(UINT(vertices.size()),0);
	ID3D11Buffer* none=nullptr; context->SOSetTargets(1,&none,&offset);
	D3D11_BUFFER_DESC stagingDesc; output->GetDesc(&stagingDesc);
	stagingDesc.Usage=D3D11_USAGE_STAGING; stagingDesc.BindFlags=0; stagingDesc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
	ComPtr<ID3D11Buffer> staging; Check(device->CreateBuffer(&stagingDesc,nullptr,&staging));
	context->CopyResource(staging.Get(),output.Get());
	D3D11_MAPPED_SUBRESOURCE readback; Check(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&readback));
	float maxError=0;
	for(std::size_t v=0;v<vertices.size();++v)
	{
		const auto& vertex=vertices[v];
		auto position=dx::XMVectorSetW(dx::XMLoadFloat3(&vertex.position),1);
		auto normal=dx::XMLoadFloat3(&vertex.normal),tangent=dx::XMLoadFloat3(&vertex.tangent);
		const float weights[]={vertex.weights.x,vertex.weights.y,vertex.weights.z,vertex.weights.w};
		const UINT indices[]={vertex.indices.x,vertex.indices.y,vertex.indices.z,vertex.indices.w};
		if(weights[0]+weights[1]+weights[2]+weights[3]>0)
		{
			auto p=dx::XMVectorZero(),n=dx::XMVectorZero(),t=dx::XMVectorZero();
			for(int i=0;i<4;++i)
			{
				const auto bone=dx::XMLoadFloat4x4(&palette[indices[i]]);
				p+=dx::XMVector4Transform(position,bone)*weights[i];
				n+=dx::XMVector3TransformNormal(normal,dx::XMMatrixTranspose(dx::XMMatrixInverse(nullptr,bone)))*weights[i];
				t+=dx::XMVector3TransformNormal(tangent,bone)*weights[i];
			}
			position=p;normal=n;tangent=t;
		}
		dx::XMFLOAT4 projected; dx::XMStoreFloat4(&projected,dx::XMVector4Transform(position,model*viewProjection));
		const auto* actual=reinterpret_cast<const float*>(static_cast<const char*>(readback.pData)+v*outputStride);
		std::vector<float> expected;
		if(!shadow)
		{
			dx::XMFLOAT3 world,n,t;
			dx::XMStoreFloat3(&world,dx::XMVector4Transform(position,model));
			normal=dx::XMVector3Normalize(dx::XMVector3TransformNormal(normal,dx::XMMatrixTranspose(dx::XMMatrixInverse(nullptr,model))));
			dx::XMStoreFloat3(&n,normal);
			expected={world.x,world.y,world.z,n.x,n.y,n.z};
			if(textured)
			{
				tangent=dx::XMVector3TransformNormal(tangent,model);
				tangent=dx::XMVector3Normalize(tangent-normal*dx::XMVector3Dot(tangent,normal)); dx::XMStoreFloat3(&t,tangent);
				expected.insert(expected.end(),{t.x,t.y,t.z,vertex.uv.x,vertex.uv.y});
			}
		}
		expected.insert(expected.end(),{projected.x,projected.y,projected.z,projected.w});
		maxError=(std::max)(maxError,Compare(actual,expected.data(),expected.size()));
	}
	context->Unmap(staging.Get(),0); context->ClearState();
	if(maxError>1e-4f) throw std::runtime_error("GPU/CPU skinning mismatch: "+std::to_string(maxError));
	std::wcout<<L"PASS "<<file<<L": max GPU error = "<<maxError<<L'\n';
}
}
int main()
{
	try
	{
		ComPtr<ID3D11Device> device; ComPtr<ID3D11DeviceContext> context;
		auto hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context);
		if(FAILED(hr)) { std::cout<<"Using WARP for GPU pipeline tests\n"; Check(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)); }
		else std::cout<<"Using hardware D3D11 device\n";
		std::vector<dx::XMFLOAT4X4> palette(Animation::MaxBones,Animation::Identity());
		dx::XMStoreFloat4x4(&palette[0],dx::XMMatrixScaling(2,1,.5f)*dx::XMMatrixRotationZ(.5f)*dx::XMMatrixTranslation(3,2,1));
		dx::XMStoreFloat4x4(&palette[1],dx::XMMatrixRotationX(-.3f)*dx::XMMatrixTranslation(-2,1,4));
		dx::XMStoreFloat4x4(&palette[127],dx::XMMatrixTranslation(1,-2,3));
		std::vector<Vertex> vertices={
			{{1,2,3},{0,1,0},{.2f,.8f},{1,0,0},{0,1,0,0},{.4f,.6f,0,0}},
			{{-1,0,2},{0,1,0},{0,1},{1,0,0},{127,0,0,0},{1,0,0,0}},
			{{3,1,-1},{0,1,0},{1,0},{1,0,0},{0,0,0,0},{0,0,0,0}},
			{{0,1,2},{0,1,0},{.5f,.5f},{1,0,0},{0,1,2,127},{.2f,.3f,.1f,.4f}}};
		Run(device.Get(),context.Get(),L"SkinnedPhongVS.cso",false,false,vertices,palette);
		Run(device.Get(),context.Get(),L"SkinnedTexturedPhongVS.cso",true,false,vertices,palette);
		Run(device.Get(),context.Get(),L"SkinnedShadowMapVS.cso",false,true,vertices,palette);
		return 0;
	}
	catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
