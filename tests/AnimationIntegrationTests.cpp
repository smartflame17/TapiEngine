#include "../Scene/GameObject.h"
#include "../Components/Animator.h"
#include "../Graphics/Drawable/Model.h"
#include "../Graphics/Drawable/Primitive.h"
#include "../Graphics/Camera.h"
#include "../Graphics/Renderer.h"
#include "../Graphics/ShadowDrawContext.h"
#include "../Graphics/IBindable/ShadowTransformCbuf.h"
#include "../Graphics/Lighting/DirectionalLight.h"
#include "../Graphics/Lighting/SpotLight.h"
#include "../Graphics/Lighting/PointLight.h"
#include <d3d11sdklayers.h>
#include <cmath>
#include <fstream>
#include <iostream>

namespace dx = DirectX;
using Microsoft::WRL::ComPtr;
namespace
{
int checks = 0;
void Check(bool condition, const char* message)
{
	++checks;
	if (!condition) throw std::runtime_error(message);
}
struct Access : IBindable
{
	using IBindable::GetContext;
	using IBindable::GetDevice;
	void Bind(Graphics&) noexcept override {}
};
std::shared_ptr<ModelAsset> MakeAsset(bool textured = false)
{
	auto asset = std::make_shared<ModelAsset>();
	Animation::SkeletonNode root, meshNode, bone;
	root.name = "root"; meshNode.name = "mesh"; meshNode.parent = 0; meshNode.meshes = { 0 };
	bone.name = "bone"; bone.parent = 0;
	asset->skeleton.nodes = { root, meshNode, bone }; asset->skeleton.boneNodes = { 2 };
	MeshAsset mesh; mesh.name = "fixture triangle"; mesh.hasUV = textured;
	mesh.vertices = {
		{{-.4f,0,0},{0,0,-1},{1,0,0},{0,1},{{0,0,0,0},{1,0,0,0}}},
		{{0,.8f,0},{0,0,-1},{1,0,0},{.5f,0},{{0,0,0,0},{1,0,0,0}}},
		{{.4f,0,0},{0,0,-1},{1,0,0},{1,1},{{0,0,0,0},{1,0,0,0}}}};
	mesh.indices = { 0,1,2 };
	mesh.bounds.Center = {0,.4f,0}; mesh.bounds.Extents = {.4f,.4f,.001f};
	mesh.skin.boneNodes = {2}; mesh.skin.inverseBinds = {Animation::Identity()};
	mesh.skin.influenceBounds = {mesh.bounds}; mesh.skin.hasInfluenceBounds = {true};
	asset->meshes.push_back(std::move(mesh)); return asset;
}
std::shared_ptr<Animation::AnimationClip> MakeClip(const ModelAsset& asset, const char* id, float movement)
{
	auto clip = std::make_shared<Animation::AnimationClip>();
	clip->id = id; clip->label = id; clip->duration = 1; clip->sourceRig = asset.skeleton;
	Animation::NodeTrack track; track.nodeName = "bone";
	track.translations = {{0,{0,0,0}}, {1,{movement,1,0}}};
	clip->tracks.push_back(track); return clip;
}
Model& AttachModel(GameObject& object, Graphics& gfx, std::shared_ptr<const ModelAsset> asset)
{
	auto model = std::make_unique<Model>(gfx, std::move(asset)); auto* result = model.get();
	object.AddComponent<DrawableComponent>(std::move(model)); return *result;
}
void TestComponents(Graphics& gfx)
{
	Scene scene;
	auto asset = MakeAsset();
	auto clip = MakeClip(*asset,"walk",3);
	auto jump = MakeClip(*asset,"jump",-2);
	auto& first = scene.CreateGameObject("first"); auto& model = AttachModel(first,gfx,asset);
	auto& animator = first.AddComponent<Animator>();
	auto& second = scene.CreateGameObject("second"); second.SetPosition(-5,0,0);
	auto& secondModel = AttachModel(second,gfx,asset); auto& other = second.AddComponent<Animator>();
	Check(animator.AddClip(clip) && animator.AddClip(jump,false) && other.AddClip(clip),"Attach interchangeable clips");
	animator.Play(); scene.UpdateAnimations(.25f,false,false);
	Check(animator.GetTime()==.25 && other.GetTime()==0,"Edit playback is automatic; stopped Animators remain stopped");
	scene.UpdateAnimations(.25f,true,true); Check(animator.GetTime()==.25,"Global pause freezes preview");
	other.Play();
	scene.UpdateAnimations(.25f,true,false); Check(animator.GetTime()==.5 && other.GetTime()==.25,"Independent simulation clocks");
	Check(model.GetPose().global[2]._41 != secondModel.GetPose().global[2]._41,"Shared asset has per-instance poses");
	animator.Pause(); scene.UpdateAnimations(.2f,true,false); Check(animator.GetTime()==.5,"Animator Pause");
	animator.Play(); animator.SetSpeed(2); scene.UpdateAnimations(.1f,true,false);
	Check(std::abs(animator.GetTime()-.7)<1e-6,"Resume and speed");
	animator.SelectClip(1); Check(animator.GetTime()==0 && animator.GetState()==Animation::PlaybackState::Playing,"Switch starts at zero");
	scene.UpdateAnimations(2,true,false); Check(animator.GetState()==Animation::PlaybackState::Completed && animator.GetTime()==1,"Nonloop completion");
	animator.Restart(); scene.UpdateAnimations(0,true,false); Check(animator.GetTime()==0,"Restart samples first frame");
	animator.Seek(.75); scene.UpdateAnimations(0,true,true); Check(std::abs(model.GetPose().global[2]._41+1.5f)<1e-5,"Seek while globally paused");
	animator.Stop(); scene.UpdateAnimations(0,true,false);
	Check(model.GetPose().global[2]._41==0 && animator.GetClips().size()==2,"Stop restores bind and retains clips");
	animator.SelectClip(0); animator.Seek(.9); scene.UpdateAnimations(0,false,false);
	Check(model.GetLocalBounds().Center.x>2.5f,"Animated bounds move beyond bind bounds");
	const auto bounds = first.GetComponent<DrawableComponent>()->GetWorldBounds();
	const dx::SimpleMath::Ray ray({bounds.Center.x,bounds.Center.y,-5},{0,0,1});
	scene.SelectGameObjectByRay(ray); Check(scene.GetSelectedObject()==&first,"Picking uses animated bounds");
	Camera camera; camera.SetPosition(2.7f,1,-3);
	RenderView view; view.camera=&camera; view.projection=dx::XMMatrixPerspectiveFovLH(.5f,1,.1f,10);
	RenderQueue queue; RenderQueueBuilder builder(queue,view); scene.Submit(builder,view);
	Check(std::any_of(queue.GetOpaqueItems().begin(),queue.GetOpaqueItems().end(),[&](const auto& item){return item.drawable==&model;}),"Frustum culling uses animated bounds");
	Check(camera.GetFrustum().Contains(asset->meshes[0].bounds)==dx::DISJOINT,"Bind bounds are outside the test camera");
	animator.Stop();scene.UpdateAnimations(0,false,false);queue.Reset();scene.Submit(builder,view);
	Check(std::none_of(queue.GetOpaqueItems().begin(),queue.GetOpaqueItems().end(),[&](const auto& item){return item.drawable==&model;}),"Restored bind bounds are culled");
	animator.SetSpeed(1);animator.Play();scene.UpdateAnimations(.9f,false,false);queue.Reset();scene.Submit(builder,view);
	Check(std::any_of(queue.GetOpaqueItems().begin(),queue.GetOpaqueItems().end(),[&](const auto& item){return item.drawable==&model;}),"Offscreen animation advances back into the frustum");
	animator.Pause();
	const auto time=animator.GetTime(); const auto count=animator.GetClips().size();
	auto invalid=MakeClip(*asset,"invalid",1); invalid->sourceRig.nodes[2].name="wrong";
	Check(!animator.AddClip(invalid) && animator.GetClips().size()==count && animator.GetTime()==time,"Failed add preserves playback");
	Check(!animator.LoadAnimations("__missing__.fbx") && animator.GetClips().size()==count,"Failed import preserves clips");
	animator.RemoveClip(1); Check(animator.GetTime()==time,"Removing inactive clip preserves playback");
	animator.RemoveClip(0); scene.UpdateAnimations(0,false,false); Check(model.GetPose().global[2]._41==0,"Removing active clip restores bind");
	animator.AddClip(clip); animator.Play(); scene.UpdateAnimations(.25f,true,false);
	bool duplicateRejected=false; try { first.AddComponent<Animator>(); } catch(const std::logic_error&) { duplicateRejected=true; }
	Check(duplicateRejected,"Duplicate Animator rejected");
	first.RemoveComponent<Animator>(); scene.UpdateAnimations(0,false,false);
	Check(model.GetPose().global[2]._41==0,"Pending Animator removal immediately restores bind");
	scene.CleanupPendingComponentRemovals(); scene.UpdateAnimations(0,false,false);
	Check(model.GetPose().global[2]._41==0,"Animator removal restores bind without dangling pose");
	auto& replacement=first.AddComponent<Animator>(); replacement.AddClip(clip); replacement.Play();
	const Drawable* removedDrawable=&model;
	first.RemoveComponent<DrawableComponent>(); scene.CleanupPendingComponentRemovals(); scene.UpdateAnimations(.1f,true,false);
	Check(replacement.GetTime()==0,"Missing drawable disables advancement");
	queue.Reset();scene.Submit(builder,view);
	Check(std::none_of(queue.GetOpaqueItems().begin(),queue.GetOpaqueItems().end(),[&](const auto& item){return item.drawable==removedDrawable;}),"Removed drawable leaves the BVH and render queue");
	scene.SelectGameObjectByRay(ray);Check(scene.GetSelectedObject()!=&first,"Removed drawable is no longer pickable");
	auto& replacedModel=AttachModel(first,gfx,asset); scene.UpdateAnimations(.2f,true,false);
	Check(replacedModel.GetPose().global[2]._41>.5f,"Replacement drawable rebinds safely");
	AttachModel(first,gfx,asset); const auto before=replacement.GetTime(); scene.UpdateAnimations(.2f,true,false);
	Check(replacement.GetTime()==before && replacedModel.GetPose().global[2]._41==0,"Ambiguous target disables and resets pose");
	// Exercise the actual inspector layout for valid and invalid dependencies.
	auto& io=ImGui::GetIO(); io.DisplaySize={1600,900}; io.DeltaTime=1.0f/60;
	unsigned char* pixels; int w,h; io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
	ImGui::NewFrame(); ImGui::Begin("Integration Inspector"); replacement.OnInspector(); other.OnInspector(); ImGui::End(); ImGui::Render();
	second.Destroy(); scene.CleanupDestroyedObjects(); scene.UpdateAnimations(.1f,true,false);
	std::cout<<"PASS component lifecycle, controls, inspector, animated bounds and culling\n";
}

std::vector<unsigned char> Capture(Graphics& gfx, const std::filesystem::path& file)
{
	ComPtr<ID3D11RenderTargetView> target; Access::GetContext(gfx)->OMGetRenderTargets(1,&target,nullptr);
	ComPtr<ID3D11Resource> resource; target->GetResource(&resource);
	ComPtr<ID3D11Texture2D> texture; resource.As(&texture);
	D3D11_TEXTURE2D_DESC desc; texture->GetDesc(&desc);
	desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;desc.MiscFlags=0;
	ComPtr<ID3D11Texture2D> staging;
	Check(SUCCEEDED(Access::GetDevice(gfx)->CreateTexture2D(&desc,nullptr,&staging)),"Capture staging texture");
	Access::GetContext(gfx)->CopyResource(staging.Get(),texture.Get());
	D3D11_MAPPED_SUBRESOURCE mapped;
	Check(SUCCEEDED(Access::GetContext(gfx)->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped)),"Capture readback");
	std::vector<unsigned char> rgb;
	for(UINT y=60;y<780;++y) for(UINT x=300;x<1580;++x)
	{
		const auto* pixel=static_cast<const unsigned char*>(mapped.pData)+y*mapped.RowPitch+x*4;
		rgb.insert(rgb.end(),{pixel[2],pixel[1],pixel[0]});
	}
	Access::GetContext(gfx)->Unmap(staging.Get(),0);
	std::ofstream out(file,std::ios::binary);out<<"P6\n1280 720\n255\n";out.write(reinterpret_cast<const char*>(rgb.data()),rgb.size());
	return rgb;
}
std::pair<double, std::size_t> ShadowCentroid(Graphics& gfx)
{
	ComPtr<ID3D11DepthStencilView> view; Access::GetContext(gfx)->OMGetRenderTargets(0,nullptr,&view);
	ComPtr<ID3D11Resource> resource; view->GetResource(&resource);
	ComPtr<ID3D11Texture2D> texture; resource.As(&texture);
	D3D11_TEXTURE2D_DESC desc; texture->GetDesc(&desc);
	D3D11_DEPTH_STENCIL_VIEW_DESC viewDesc; view->GetDesc(&viewDesc);
	const UINT slice=viewDesc.ViewDimension==D3D11_DSV_DIMENSION_TEXTURE2DARRAY?viewDesc.Texture2DArray.FirstArraySlice:0;
	desc.Usage=D3D11_USAGE_STAGING;desc.ArraySize=1;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;desc.MiscFlags=0;
	ComPtr<ID3D11Texture2D> staging;
	Check(SUCCEEDED(Access::GetDevice(gfx)->CreateTexture2D(&desc,nullptr,&staging)),"Shadow readback buffer");
	Access::GetContext(gfx)->CopySubresourceRegion(staging.Get(),0,0,0,0,texture.Get(),slice,nullptr);
	D3D11_MAPPED_SUBRESOURCE mapped; Check(SUCCEEDED(Access::GetContext(gfx)->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped)),"Shadow depth readback");
	double sum=0;std::size_t pixels=0;
	for(UINT y=0;y<desc.Height;++y)
	{
		const auto* row=reinterpret_cast<const std::uint32_t*>(static_cast<const char*>(mapped.pData)+y*mapped.RowPitch);
		for(UINT x=0;x<desc.Width;++x) if((row[x]&0xffffffu)<0xffff00u){sum+=x;++pixels;}
	}
	Access::GetContext(gfx)->Unmap(staging.Get(),0);
	Check(pixels>10,"Shadow contains rendered geometry");return {sum/pixels,pixels};
}
void TestShadows(Graphics& gfx)
{
	auto asset=MakeAsset(); Model model(gfx,asset);
	Primitive rigid(gfx,Primitive::Shape::Cube,Primitive::SurfaceMode::Material);
	VertexShader staticVS(gfx,L"ShadowMapVS.cso"),skinVS(gfx,L"SkinnedShadowMapVS.cso");
	ShadowDrawContext context{staticVS,skinVS};
	ShadowMap map(gfx,128,2,ShadowMap::Type::TextureCube);
	ShadowTransformCbuf::SetLightViewProjection(dx::XMMatrixOrthographicLH(10,10,.1f,100));
	model.SetExternalTransformMatrix(dx::XMMatrixTranslation(0,0,5)); rigid.SetExternalTransformMatrix(dx::XMMatrixTranslation(2,0,5));
	gfx.UnbindPixelShader();
	auto clip=MakeClip(*asset,"shadow-motion",3);auto binding=Animation::BindClip(asset->skeleton,*clip);
	Animation::SkeletonPose animated;Animation::EvaluatePose(asset->skeleton,*clip,binding,1,animated);
	for(UINT face=0;face<6;++face)
	{
		model.ResetPose();
		map.BeginWriteFace(gfx,face);
		model.DrawShadow(gfx,context);
		const auto before=ShadowCentroid(gfx);
		model.SetPose(animated);map.BeginWriteFace(gfx,face);model.DrawShadow(gfx,context);
		const auto after=ShadowCentroid(gfx);
		Check(after.first-before.first>30,"Shadow silhouette follows animated position on cube face");
		ComPtr<ID3D11VertexShader> bound; Access::GetContext(gfx)->VSGetShader(&bound,nullptr,nullptr);
		skinVS.Bind(gfx); ComPtr<ID3D11VertexShader> expected; Access::GetContext(gfx)->VSGetShader(&expected,nullptr,nullptr);
		Check(bound.Get()==expected.Get(),"Skinned shadow shader selected");
		rigid.DrawShadow(gfx,context);Access::GetContext(gfx)->VSGetShader(bound.ReleaseAndGetAddressOf(),nullptr,nullptr);
		staticVS.Bind(gfx);Access::GetContext(gfx)->VSGetShader(expected.ReleaseAndGetAddressOf(),nullptr,nullptr);
		Check(bound.Get()==expected.Get(),"Rigid shader restored after skinned shadow");
		model.DrawShadow(gfx,context);
	}
	ShadowMap directional(gfx,128,2,ShadowMap::Type::Texture2D);
	model.ResetPose();directional.BeginWrite(gfx);model.DrawShadow(gfx,context);const auto before=ShadowCentroid(gfx);
	model.SetPose(animated);directional.BeginWrite(gfx);model.DrawShadow(gfx,context);const auto after=ShadowCentroid(gfx);
	Check(after.first-before.first>30,"2D shadow silhouette follows animated position");
	gfx.RestoreDefaultStates();
	std::cout<<"PASS alternating rigid/skinned shadow draws on six cube faces\n";
}
void TestRendering(Graphics& gfx,const std::filesystem::path& out)
{
	if(!std::filesystem::exists("Graphics/Models/Humanoid/Body.fbx")) { std::cout<<"SKIP local humanoid render\n"; return; }
	Scene scene;
	auto body=ImportModel("Graphics/Models/Humanoid/Body.fbx");
	auto& object=scene.CreateGameObject("humanoid");auto& model=AttachModel(object,gfx,body);object.SetScale(.007f,.007f,.007f);
	auto& animator=object.AddComponent<Animator>();
	Check(animator.LoadAnimations("Graphics/Models/Humanoid/Animations/Walking.fbx"),"Load Walking into Animator");
	Check(animator.LoadAnimations("Graphics/Models/Humanoid/Animations/Jump.fbx",false),"Load Jump into Animator");
	auto& floor=scene.CreateGameObject("floor");floor.SetScale(8,.1f,8);floor.SetPosition(0,-.15f,0);
	floor.AddComponent<DrawableComponent>(std::make_unique<Primitive>(gfx,Primitive::Shape::Cube,Primitive::SurfaceMode::Material));
	auto& cube=scene.CreateGameObject("static textured cube");cube.SetPosition(-1,.25f,.5f);cube.SetScale(.2f,.2f,.2f);
	cube.AddComponent<DrawableComponent>(std::make_unique<Primitive>(gfx,Primitive::Shape::Cube,Primitive::SurfaceMode::Textured));
	auto& directional=scene.CreateGameObject("directional");directional.SetRotation(.8f,.4f,0);directional.AddComponent<DirectionalLight>(gfx);
	auto& spot=scene.CreateGameObject("spot");spot.SetPosition(0,3,-3);spot.SetRotation(.7f,0,0);spot.AddComponent<SpotLight>(gfx).SetIntensity(.7f);
	auto& point=scene.CreateGameObject("point");point.SetPosition(2,2,-1);point.AddComponent<PointLight>(gfx).SetIntensity(.7f);
	Camera camera;camera.SetPosition(0,1.0f,-3.5f);camera.SetRotation(.08f,0,0);
	gfx.SetProjection(dx::XMMatrixPerspectiveFovLH(.8f,1280.0f/720,.05f,30));gfx.SetCamera(camera.GetViewMatrix());
	Renderer renderer(gfx);
	auto render=[&](const char* name){scene.UpdateAnimations(0,false,false);gfx.BeginFrame(.1f,.14f,.18f);renderer.Render(scene,&camera);return Capture(gfx,out/name);};
	const auto bind=render("humanoid-bind.ppm");
	animator.Seek(.5);const auto walk=render("humanoid-walk.ppm");
	animator.SelectClip(1);animator.Seek(1.05);const auto jump=render("humanoid-jump.ppm");
	std::size_t changes=0;for(std::size_t i=0;i<walk.size();++i) if(walk[i]!=jump[i]) ++changes;
	Check(changes>1000 && bind!=walk,"Humanoid rendered poses differ");
	// CPU skinning reference must remain inside the conservative animated AABB.
	for(const auto& node:body->skeleton.nodes) for(auto meshIndex:node.meshes)
	{
		const auto& mesh=body->meshes[meshIndex];const auto& skin=mesh.skin;
		for(const auto& vertex:mesh.vertices)
		{
			auto position=dx::XMVectorZero();
			const UINT ids[]={vertex.skin.indices.x,vertex.skin.indices.y,vertex.skin.indices.z,vertex.skin.indices.w};
			const float weights[]={vertex.skin.weights.x,vertex.skin.weights.y,vertex.skin.weights.z,vertex.skin.weights.w};
			for(int i=0;i<4;++i) if(weights[i]>0)
			{
				const auto matrix=dx::XMLoadFloat4x4(&skin.inverseBinds[ids[i]])*dx::XMLoadFloat4x4(&model.GetPose().global[skin.boneNodes[ids[i]]]);
				position=dx::XMVectorAdd(position,dx::XMVectorScale(dx::XMVector3Transform(dx::XMLoadFloat3(&vertex.position),matrix),weights[i]));
			}
			Check(model.GetLocalBounds().Contains(position)!=dx::DISJOINT,"Animated bounds contain skinned vertices");
		}
	}
	std::cout<<"PASS real humanoid render, three light/shadow types, static geometry, bounds containment\n";
}
}
int main(int argc,char** argv)
{
	try
	{
		const std::filesystem::path out=argc>1?argv[1]:"x64/AnimationTests";std::filesystem::create_directories(out);
		WNDCLASSW wc={};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandle(nullptr);wc.lpszClassName=L"AnimationIntegrationHiddenWindow";
		RegisterClassW(&wc);RECT rect={0,0,1600,900};AdjustWindowRect(&rect,WS_OVERLAPPEDWINDOW,FALSE);
		HWND hwnd=CreateWindowW(wc.lpszClassName,L"Animation Tests",WS_OVERLAPPEDWINDOW,0,0,rect.right-rect.left,rect.bottom-rect.top,nullptr,nullptr,wc.hInstance,nullptr);
		Check(hwnd!=nullptr,"Hidden test window created");
		ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;
		{
			Graphics gfx(hwnd,1600,900);gfx.DisableImGui();
			ComPtr<ID3D11InfoQueue> diagnostics;Access::GetDevice(gfx)->QueryInterface(IID_PPV_ARGS(&diagnostics));
			if(diagnostics) diagnostics->ClearStoredMessages();
			TestComponents(gfx);TestShadows(gfx);TestRendering(gfx,out);
			if(diagnostics)
			{
				for(UINT64 i=0;i<diagnostics->GetNumStoredMessages();++i)
				{
					SIZE_T size=0;diagnostics->GetMessage(i,nullptr,&size);std::vector<char> bytes(size);
					auto* message=reinterpret_cast<D3D11_MESSAGE*>(bytes.data());diagnostics->GetMessage(i,message,&size);
					if(message->Severity<=D3D11_MESSAGE_SEVERITY_ERROR) throw std::runtime_error(message->pDescription);
				}
			}
		}
		ImGui::DestroyContext();DestroyWindow(hwnd);
		std::cout<<"PASS "<<checks<<" integration checks\n";return 0;
	}
	catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
