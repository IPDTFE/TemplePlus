
#include "infrastructure/infrastructure.h"
#include "infrastructure/images.h"
#include "infrastructure/vfs.h"
#include "graphics/device.h"
#include "graphics/materials.h"
#include "graphics/dynamictexture.h"

namespace gfx {

	static constexpr int minTexWidth = 1024;
	static constexpr int minTexHeight = 1024;

	RenderingDevice *renderingDevice = nullptr;
	
	ResourceListener::~ResourceListener() {
	}

	ResourceListenerRegistration::ResourceListenerRegistration(RenderingDevice& device, 
		ResourceListener* listener) : mDevice(device), mListener(listener) {
		mDevice.AddResourceListener(listener);
	}

	ResourceListenerRegistration::~ResourceListenerRegistration() {
		mDevice.RemoveResourceListener(mListener);
	}

	static const char *FormatToStr(D3DFORMAT format) {
		switch (format) {
		case D3DFMT_UNKNOWN:
			return "D3DFMT_UNKNOWN";
		case D3DFMT_R8G8B8:
			return "D3DFMT_R8G8B8";
		case D3DFMT_A8R8G8B8:
			return "D3DFMT_A8R8G8B8";
		case D3DFMT_X8R8G8B8:
			return "D3DFMT_X8R8G8B8";
		case D3DFMT_R5G6B5:
			return "D3DFMT_R5G6B5";
		case D3DFMT_X1R5G5B5:
			return "D3DFMT_X1R5G5B5";
		case D3DFMT_A1R5G5B5:
			return "D3DFMT_A1R5G5B5";
		case D3DFMT_A4R4G4B4:
			return "D3DFMT_A4R4G4B4";
		case D3DFMT_R3G3B2:
			return "D3DFMT_R3G3B2";
		case D3DFMT_A8:
			return "D3DFMT_A8";
		case D3DFMT_A8R3G3B2:
			return "D3DFMT_A8R3G3B2";
		case D3DFMT_X4R4G4B4:
			return "D3DFMT_X4R4G4B4";
		case D3DFMT_A2B10G10R10:
			return "D3DFMT_A2B10G10R10";
		case D3DFMT_A8B8G8R8:
			return "D3DFMT_A8B8G8R8";
		case D3DFMT_X8B8G8R8:
			return "D3DFMT_X8B8G8R8";
		case D3DFMT_G16R16:
			return "D3DFMT_G16R16";
		case D3DFMT_A2R10G10B10:
			return "D3DFMT_A2R10G10B10";
		case D3DFMT_A16B16G16R16:
			return "D3DFMT_A16B16G16R16";
		case D3DFMT_A8P8:
			return "D3DFMT_A8P8";
		case D3DFMT_P8:
			return "D3DFMT_P8";
		case D3DFMT_L8:
			return "D3DFMT_L8";
		case D3DFMT_A8L8:
			return "D3DFMT_A8L8";
		case D3DFMT_A4L4:
			return "D3DFMT_A4L4";
		case D3DFMT_V8U8:
			return "D3DFMT_V8U8";
		case D3DFMT_L6V5U5:
			return "D3DFMT_L6V5U5";
		case D3DFMT_X8L8V8U8:
			return "D3DFMT_X8L8V8U8";
		case D3DFMT_Q8W8V8U8:
			return "D3DFMT_Q8W8V8U8";
		case D3DFMT_V16U16:
			return "D3DFMT_V16U16";
		case D3DFMT_A2W10V10U10:
			return "D3DFMT_A2W10V10U10";
		case D3DFMT_UYVY:
			return "D3DFMT_UYVY";
		case D3DFMT_R8G8_B8G8:
			return "D3DFMT_R8G8_B8G8";
		case D3DFMT_YUY2:
			return "D3DFMT_YUY2";
		case D3DFMT_G8R8_G8B8:
			return "D3DFMT_G8R8_G8B8";
		case D3DFMT_DXT1:
			return "D3DFMT_DXT1";
		case D3DFMT_DXT2:
			return "D3DFMT_DXT2";
		case D3DFMT_DXT3:
			return "D3DFMT_DXT3";
		case D3DFMT_DXT4:
			return "D3DFMT_DXT4";
		case D3DFMT_DXT5:
			return "D3DFMT_DXT5";
		case D3DFMT_D16_LOCKABLE:
			return "D3DFMT_D16_LOCKABLE";
		case D3DFMT_D32:
			return "D3DFMT_D32";
		case D3DFMT_D15S1:
			return "D3DFMT_D15S1";
		case D3DFMT_D24S8:
			return "D3DFMT_D24S8";
		case D3DFMT_D24X8:
			return "D3DFMT_D24X8";
		case D3DFMT_D24X4S4:
			return "D3DFMT_D24X4S4";
		case D3DFMT_D16:
			return "D3DFMT_D16";
		case D3DFMT_D32F_LOCKABLE:
			return "D3DFMT_D32F_LOCKABLE";
		case D3DFMT_D24FS8:
			return "D3DFMT_D24FS8";
		case D3DFMT_D32_LOCKABLE:
			return "D3DFMT_D32_LOCKABLE";
		case D3DFMT_S8_LOCKABLE:
			return "D3DFMT_S8_LOCKABLE";
		case D3DFMT_L16:
			return "D3DFMT_L16";
		case D3DFMT_VERTEXDATA:
			return "D3DFMT_VERTEXDATA";
		case D3DFMT_INDEX16:
			return "D3DFMT_INDEX16";
		case D3DFMT_INDEX32:
			return "D3DFMT_INDEX32";
		case D3DFMT_Q16W16V16U16:
			return "D3DFMT_Q16W16V16U16";
		case D3DFMT_MULTI2_ARGB8:
			return "D3DFMT_MULTI2_ARGB8";
		case D3DFMT_R16F:
			return "D3DFMT_R16F";
		case D3DFMT_G16R16F:
			return "D3DFMT_G16R16F";
		case D3DFMT_A16B16G16R16F:
			return "D3DFMT_A16B16G16R16F";
		case D3DFMT_R32F:
			return "D3DFMT_R32F";
		case D3DFMT_G32R32F:
			return "D3DFMT_G32R32F";
		case D3DFMT_A32B32G32R32F:
			return "D3DFMT_A32B32G32R32F";
		case D3DFMT_CxV8U8:
			return "D3DFMT_CxV8U8";
		case D3DFMT_A1:
			return "D3DFMT_A1";
		case D3DFMT_A2B10G10R10_XR_BIAS:
			return "D3DFMT_A2B10G10R10_XR_BIAS";
		case D3DFMT_BINARYBUFFER:
			return "D3DFMT_BINARYBUFFER";
		default:
			return "Unknown Format";
		}
	}
	
	RenderingDevice::RenderingDevice(HWND windowHandle, int renderWidth, int renderHeight, bool antiAliasing) 
		  : mWindowHandle(windowHandle), 
			mRenderWidth(renderWidth),
			mRenderHeight(renderHeight),
			mShaders(*this),
			mTextures(*this, 128 * 1024 * 1024),
			mAntiAliasing(antiAliasing) {
		Expects(!renderingDevice);
		renderingDevice = this;
		
		HRESULT status;

		status = D3DLOG(Direct3DCreate9Ex(D3D_SDK_VERSION, &mDirect3d9));
		if (status != D3D_OK) {
			throw TempleException("Unable to create Direct3D9Ex interface.");
		}

		// At this point we only do a GetDisplayMode to check the resolution. We could also do this elsewhere
		D3DDISPLAYMODE displayMode;
		status = D3DLOG(mDirect3d9->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &displayMode));
		if (status != D3D_OK) {
			throw TempleException("Unable to query display mode for primary adapter.");
		}

		logger->info("Display format: {}x{}@{} Format: {}", displayMode.Width, displayMode.Height, 
			displayMode.RefreshRate, FormatToStr(displayMode.Format));

		D3DMULTISAMPLE_TYPE aaTypes[] = {
			D3DMULTISAMPLE_2_SAMPLES,
			D3DMULTISAMPLE_3_SAMPLES,
			D3DMULTISAMPLE_4_SAMPLES,
			D3DMULTISAMPLE_5_SAMPLES,
			D3DMULTISAMPLE_6_SAMPLES,
			D3DMULTISAMPLE_7_SAMPLES,
			D3DMULTISAMPLE_8_SAMPLES,
			D3DMULTISAMPLE_9_SAMPLES,
			D3DMULTISAMPLE_10_SAMPLES,
			D3DMULTISAMPLE_11_SAMPLES,
			D3DMULTISAMPLE_12_SAMPLES,
			D3DMULTISAMPLE_13_SAMPLES,
			D3DMULTISAMPLE_14_SAMPLES,
			D3DMULTISAMPLE_15_SAMPLES,
			D3DMULTISAMPLE_16_SAMPLES
		};

		for (auto type : aaTypes) {
			status = mDirect3d9->CheckDeviceMultiSampleType(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, TRUE, type, nullptr);
			if (status == D3D_OK) {
				logger->info("AA method {} is available", type);
				mSupportedAaSamples.push_back(type);
			}
		}

		// We need at least 1024x768
		if (displayMode.Width < 1024 || displayMode.Height < 768) {
			throw TempleException("You need at least a display resolution of 1024x768.");
		}

		CreatePresentParams();

		// Read preliminary caps needed to create the device
		D3DCAPS9 caps;
		if (mDirect3d9->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &caps) != D3D_OK) {
			throw TempleException("Unable to retrieve the caps for the default device.");
		}

		auto vertexProcessing = D3DCREATE_HARDWARE_VERTEXPROCESSING;
		if (!(caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT)) {
			logger->info("Device does not support hardware T&L. Falling back to software T&L");
			vertexProcessing = D3DCREATE_SOFTWARE_VERTEXPROCESSING;
		}

		status = D3DLOG(mDirect3d9->CreateDeviceEx(
			D3DADAPTER_DEFAULT,
			D3DDEVTYPE_HAL,
			mWindowHandle,
			vertexProcessing,
			&mPresentParams,
			nullptr,
			&mDevice));

		if (status != D3D_OK) {
			throw TempleException("Unable to create Direct3D9 device!");
		}

		// TODO: color bullshit is not yet done (tig_d3d_init_handleformat et al)

		AfterDeviceResetOrCreated();

		SetRenderSize(renderWidth, renderHeight);
		
		for (auto &listener : mResourcesListeners) {
			listener->CreateResources(*this);
		}
		mResourcesCreated = true;
	}

	RenderingDevice::RenderingDevice(IDirect3DDevice9Ex *device, int renderWidth, int renderHeight)
		: mDevice(device),
		mRenderWidth(renderWidth),
		mRenderHeight(renderHeight),
		mShaders(*this),
		mTextures(*this, 128 * 1024 * 1024) {
		Expects(!renderingDevice);
		renderingDevice = this;

		mCamera.SetScreenWidth((float)renderWidth, (float)renderHeight);

		// TODO: color bullshit is not yet done (tig_d3d_init_handleformat et al)

		// Get the device caps for real this time.
		ReadCaps();

		// Get the currently attached backbuffer
		if (D3DLOG(mDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &mBackBuffer)) != D3D_OK) {
			throw TempleException("Unable to retrieve the back buffer");
		}
		if (D3DLOG(mDevice->GetDepthStencilSurface(&mBackBufferDepth)) != D3D_OK) {
			throw TempleException("Unable to retrieve depth/stencil surface from device");
		}

		memset(&mBackBufferDesc, 0, sizeof(mBackBufferDesc));
		if (D3DLOG(mBackBuffer->GetDesc(&mBackBufferDesc)) != D3D_OK) {
			throw TempleException("Unable to retrieve back buffer description");
		}

		// Create surfaces for the scene
		D3DLOG(mDevice->CreateRenderTarget(
			mRenderWidth,
			mRenderHeight,
			mBackBufferDesc.Format,
			D3DMULTISAMPLE_16_SAMPLES,
			0,
			FALSE,
			&mSceneSurface,
			nullptr));

		D3DLOG(mDevice->CreateDepthStencilSurface(
			mRenderWidth,
			mRenderHeight,
			D3DFMT_D16,
			D3DMULTISAMPLE_16_SAMPLES,
			0,
			TRUE,
			&mSceneDepthSurface,
			nullptr));

		for (auto &listener : mResourcesListeners) {
			listener->CreateResources(*this);
		}
		mResourcesCreated = true;
	}

	RenderingDevice::~RenderingDevice() {
		renderingDevice = nullptr;
	}

	void RenderingDevice::AfterDeviceResetOrCreated()
	{
		// Get the device caps for real this time.
		ReadCaps();

		mBackBuffer.Release();
		mBackBufferDepth.Release();

		// Get the currently attached backbuffer
		if (D3DLOG(mDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &mBackBuffer)) != D3D_OK) {
			throw TempleException("Unable to retrieve the back buffer");
		}
		if (D3DLOG(mDevice->GetDepthStencilSurface(&mBackBufferDepth)) != D3D_OK) {
			throw TempleException("Unable to retrieve depth/stencil surface from device");
		}

		memset(&mBackBufferDesc, 0, sizeof(mBackBufferDesc));
		if (D3DLOG(mBackBuffer->GetDesc(&mBackBufferDesc)) != D3D_OK) {
			throw TempleException("Unable to retrieve back buffer description");
		}
	}

	void RenderingDevice::CreatePresentParams()
	{
		memset(&mPresentParams, 0, sizeof(mPresentParams));

		mPresentParams.BackBufferFormat = D3DFMT_X8R8G8B8;
		// Using discard here allows us to do multisampling.
		mPresentParams.SwapEffect = D3DSWAPEFFECT_DISCARD;
		mPresentParams.hDeviceWindow = mWindowHandle;
		mPresentParams.Windowed = true;
		mPresentParams.EnableAutoDepthStencil = true;
		mPresentParams.AutoDepthStencilFormat = D3DFMT_D16;
		mPresentParams.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
	}

	void RenderingDevice::ReadCaps()
	{

		mVideoMemory = mDevice->GetAvailableTextureMem();

		if (D3DLOG(mDevice->GetDeviceCaps(&mCaps)) != D3D_OK) {
			throw TempleException("Unable to retrieve Direct3D device mCaps");
		}

		/*
		Several sanity checks follow
		*/
		if (!(mCaps.SrcBlendCaps & D3DPBLENDCAPS_SRCALPHA)) {
			logger->error("source D3DPBLENDCAPS_SRCALPHA is missing");
		}
		if (!(mCaps.SrcBlendCaps & D3DPBLENDCAPS_ONE)) {
			logger->error("source D3DPBLENDCAPS_ONE is missing");
		}
		if (!(mCaps.SrcBlendCaps & D3DPBLENDCAPS_ZERO)) {
			logger->error("source D3DPBLENDCAPS_ZERO is missing");
		}
		if (!(mCaps.DestBlendCaps & D3DPBLENDCAPS_INVSRCALPHA)) {
			logger->error("destination D3DPBLENDCAPS_INVSRCALPHA is missing");
		}
		if (!(mCaps.DestBlendCaps & D3DPBLENDCAPS_ONE)) {
			logger->error("destination D3DPBLENDCAPS_ONE is missing");
		}
		if (!(mCaps.DestBlendCaps & D3DPBLENDCAPS_ZERO)) {
			logger->error("destination D3DPBLENDCAPS_ZERO is missing");
		}

		if (mCaps.MaxSimultaneousTextures < 4) {
			logger->error("less than 4 active textures possible: {}", mCaps.MaxSimultaneousTextures);
		}
		if (mCaps.MaxTextureBlendStages < 4) {
			logger->error("less than 4 texture blend stages possible: {}", mCaps.MaxTextureBlendStages);
		}

		if (!(mCaps.TextureOpCaps & D3DTOP_DISABLE)) {
			logger->error("texture op D3DTOP_DISABLE is missing");
		}
		if (!(mCaps.TextureOpCaps & D3DTOP_SELECTARG1)) {
			logger->error("texture op D3DTOP_SELECTARG1 is missing");
		}
		if (!(mCaps.TextureOpCaps & D3DTOP_SELECTARG2)) {
			logger->error("texture op D3DTOP_SELECTARG2 is missing");
		}
		if (!(mCaps.TextureOpCaps & D3DTOP_BLENDTEXTUREALPHA)) {
			logger->error("texture op D3DTOP_BLENDTEXTUREALPHA is missing");
		}
		if (!(mCaps.TextureOpCaps & D3DTOP_BLENDCURRENTALPHA)) {
			logger->error("texture op D3DTOP_BLENDCURRENTALPHA is missing");
		}
		if (!(mCaps.TextureOpCaps & D3DTOP_MODULATE)) {
			logger->error("texture op D3DTOP_MODULATE is missing");
		}
		if (!(mCaps.TextureOpCaps & D3DTOP_ADD)) {
			logger->error("texture op D3DTOP_ADD is missing");
		}
		if (!(mCaps.TextureOpCaps & D3DTOP_MODULATEALPHA_ADDCOLOR)) {
			logger->error("texture op D3DTOP_MODULATEALPHA_ADDCOLOR is missing");
		}
		if (mCaps.MaxTextureWidth < minTexWidth || mCaps.MaxTextureHeight < minTexHeight) {
			auto msg = fmt::format("minimum texture resolution of {}x{} is not supported. Supported: {}x{}",
				minTexWidth, minTexHeight, mCaps.MaxTextureWidth, mCaps.MaxTextureHeight);
			throw TempleException(msg);
		}

		if ((mCaps.TextureCaps & D3DPTEXTURECAPS_POW2) != 0) {
			logger->error("Textures must be power of two");
		}
		if ((mCaps.TextureCaps & D3DPTEXTURECAPS_SQUAREONLY) != 0) {
			logger->error("Textures must be square");
		}
	}

	void RenderingDevice::SetAntiAliasing(bool enable)
	{
		if (mAntiAliasing != enable) {
			mAntiAliasing = enable;
			SetRenderSize(mRenderWidth, mRenderHeight);
		}
	}

	void RenderingDevice::AddResourceListener(ResourceListener* listener) {
		mResourcesListeners.push_back(listener);
		if (mResourcesCreated) {
			listener->CreateResources(*this);
		}
	}

	void RenderingDevice::RemoveResourceListener(ResourceListener* listener) {
		mResourcesListeners.remove(listener);
		if (mResourcesCreated) {
			listener->FreeResources(*this);
		}
	}

	bool RenderingDevice::BeginFrame() {

		if (mBeginSceneDepth++ > 0) {
			return true;
		}

		auto clearColor = D3DCOLOR_ARGB(0, 0, 0, 0);

		auto result = D3DLOG(mDevice->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, clearColor, 1.0f, 0));

		if (result != D3D_OK) {
			return false;
		}

		result = D3DLOG(mDevice->BeginScene());

		if (result != D3D_OK) {
			return false;
		}

		mLastFrameStart = Clock::now();

		return true;
	}

	bool RenderingDevice::Present() {
		if (--mBeginSceneDepth > 0) {
			return true;
		}

		mTextures.FreeUnusedTextures();

		if (D3DLOG(mDevice->EndScene()) != D3D_OK) {
			return false;
		}

		auto result = mDevice->Present(nullptr, nullptr, nullptr, nullptr);

		if (result == D3DERR_DEVICEHUNG) {
				throw TempleException("TemplePlus caused your graphics driver to crash.\n"
					"You may be able to fix this by updating your graphics drivers.");
		}

		if (result != S_OK && result != S_PRESENT_OCCLUDED) {
			LogD3dError("Present()", result);
			if (result == D3DERR_DEVICELOST || result == S_PRESENT_MODE_CHANGED) {
				ResetDevice();
			}
			return false;
		}

		return true;
	}

	void RenderingDevice::ResetDevice() {
		

		CreatePresentParams();
		auto result = mDevice->Reset(&mPresentParams);
		if (result != D3D_OK) {
			logger->warn("Device reset failed.");
			return;
		}

		// Retrieve new backbuffer surface
		AfterDeviceResetOrCreated();

		// Set the cursor again, if there was one set before
		if (mCursor) {
			SetCursor(mCursorHotspot.x, mCursorHotspot.y, mCursor);
		}

	}

	void RenderingDevice::SetMaterial(const Material &material) {
		
		SetRasterizerState(material.GetRasterizerState());
		SetBlendState(material.GetBlendState());
		SetDepthStencilState(material.GetDepthStencilState());

		for (size_t i = 0; i < material.GetSamplers().size(); ++i) {
			auto& sampler = material.GetSamplers()[i];
			if (sampler.GetTexture()) {
				D3DLOG(mDevice->SetTexture(i, sampler.GetTexture()->GetDeviceTexture()));
			} else {
				D3DLOG(mDevice->SetTexture(i, nullptr));
			}
			SetSamplerState(i, sampler.GetState());
		}

		// Free up the texture bindings of the samplers currently being used
		for (size_t i = material.GetSamplers().size(); i < mUsedSamplers; ++i) {
			D3DLOG(mDevice->SetTexture(i, nullptr));
		}

		mUsedSamplers = material.GetSamplers().size();
		
		material.GetVertexShader()->Bind();
		material.GetPixelShader()->Bind();
	}

	void RenderingDevice::SetVertexShaderConstant(uint32_t startRegister, StandardSlotSemantic semantic) {
		switch (semantic) {
		case StandardSlotSemantic::ViewProjMatrix:
			mDevice->SetVertexShaderConstantF(startRegister, &mCamera.GetViewProj()._11, 4);
			break;
		default:
			break;
		}
	}

	void RenderingDevice::SetPixelShaderConstant(uint32_t startRegister, StandardSlotSemantic semantic) {
		switch (semantic) {
		case StandardSlotSemantic::ViewProjMatrix:
			mDevice->SetPixelShaderConstantF(startRegister, &mCamera.GetViewProj()._11, 4);
			break;
		default: 
			break;
		}
	}

	IndexBufferPtr RenderingDevice::CreateEmptyIndexBuffer(size_t count) {
		CComPtr<IDirect3DIndexBuffer9> buffer;

		auto length = sizeof(uint16_t) * count;
		if (D3DLOG(mDevice->CreateIndexBuffer(length,
			D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY,
			D3DFMT_INDEX16,
			D3DPOOL_DEFAULT,
			&buffer,
			nullptr)) != D3D_OK) {
			throw TempleException("Unable to create index buffer.");
		}

		return std::make_shared<IndexBuffer>(buffer, count);
	}

	VertexBufferPtr RenderingDevice::CreateEmptyVertexBuffer(size_t size, bool forPoints) {
		CComPtr<IDirect3DVertexBuffer9> buffer;

		DWORD usage = D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY;

		if (forPoints) {
			usage |= D3DUSAGE_POINTS;
		}

		if (mDevice->CreateVertexBuffer(size,
			usage,
			0,
			D3DPOOL_DEFAULT,
			&buffer,
			nullptr) != D3D_OK) {
			throw TempleException("Unable to create index buffer.");
		}

		return std::make_shared<VertexBuffer>(buffer, size);
	}

	void RenderingDevice::SetRasterizerState(const RasterizerState &state) {
		mDevice->SetRenderState(D3DRS_FILLMODE, state.fillMode);
		mDevice->SetRenderState(D3DRS_CULLMODE, state.cullMode);
	}

	void RenderingDevice::SetBlendState(const BlendState &state) {
		mDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, state.blendEnable ? TRUE : FALSE);
		mDevice->SetRenderState(D3DRS_SRCBLEND, state.srcBlend);
		mDevice->SetRenderState(D3DRS_DESTBLEND, state.destBlend);

		DWORD mask = 0;
		if (state.writeRed) {
			mask |= D3DCOLORWRITEENABLE_RED;
		}
		if (state.writeGreen) {
			mask |= D3DCOLORWRITEENABLE_GREEN;
		}
		if (state.writeBlue) {
			mask |= D3DCOLORWRITEENABLE_BLUE;
		}
		if (state.writeAlpha) {
			mask |= D3DCOLORWRITEENABLE_ALPHA;
		}
		mDevice->SetRenderState(D3DRS_COLORWRITEENABLE, mask);
	}

	void RenderingDevice::SetDepthStencilState(const DepthStencilState &state) {
		mDevice->SetRenderState(D3DRS_ZENABLE, state.depthEnable ? TRUE : FALSE);
		mDevice->SetRenderState(D3DRS_ZFUNC, state.depthFunc);
		mDevice->SetRenderState(D3DRS_ZWRITEENABLE, state.depthWrite ? TRUE : FALSE);
	}

	void RenderingDevice::SetSamplerState(int samplerIdx, const SamplerState &state) {
		mDevice->SetSamplerState(samplerIdx, D3DSAMP_MINFILTER, state.minFilter);
		mDevice->SetSamplerState(samplerIdx, D3DSAMP_MAGFILTER, state.magFilter);
		mDevice->SetSamplerState(samplerIdx, D3DSAMP_MIPFILTER, state.mipFilter);

		mDevice->SetSamplerState(samplerIdx, D3DSAMP_ADDRESSU, state.addressU);
		mDevice->SetSamplerState(samplerIdx, D3DSAMP_ADDRESSV, state.addressV);
	}

	void RenderingDevice::SetCursor(int hotspotX, int hotspotY, const gfx::TextureRef & texture)
	{
		// Save for device reset
		mCursor = texture;
		mCursorHotspot = { hotspotX, hotspotY };

		auto deviceTexture = texture->GetDeviceTexture();
		CComPtr<IDirect3DSurface9> surface;
		if (D3DLOG(deviceTexture->GetSurfaceLevel(0, &surface)) != D3D_OK) {
			logger->error("Unable to get surface of cursor texture.");
			return;
		}

		if (D3DLOG(mDevice->SetCursorProperties(hotspotX, hotspotY, surface)) != D3D_OK) {
			logger->error("Unable to set cursor properties.");
		}
	}

	VertexBufferPtr RenderingDevice::CreateVertexBufferRaw(gsl::span<const uint8_t> data) {
		CComPtr<IDirect3DVertexBuffer9> result;

		D3DLOG(mDevice->CreateVertexBuffer(data.size(), D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, &result, nullptr));

		void* dataOut;
		D3DLOG(result->Lock(0, data.size(), &dataOut, D3DLOCK_DISCARD));

		memcpy(dataOut, &data[0], data.size());

		D3DLOG(result->Unlock());

		return std::make_shared<VertexBuffer>(result, data.size());
	}

	IndexBufferPtr RenderingDevice::CreateIndexBuffer(gsl::span<const uint16_t> data) {
		CComPtr<IDirect3DIndexBuffer9> result;

		D3DLOG(mDevice->CreateIndexBuffer(data.size_bytes(),
			D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY,
			D3DFMT_INDEX16,
			D3DPOOL_DEFAULT,
			&result, nullptr));

		void* dataOut;
		D3DLOG(result->Lock(0, data.size(), &dataOut, D3DLOCK_DISCARD));

		memcpy(dataOut, &data[0], data.size() * sizeof(uint16_t));

		D3DLOG(result->Unlock());

		return std::make_shared<IndexBuffer>(result, data.size());
	}

	void RenderingDevice::SetRenderSize(int w, int h) {

		mRenderWidth = w;
		mRenderHeight = h;

		mCamera.SetScreenWidth((float) mRenderWidth, (float) mRenderHeight);

		auto widthFactor = GetScreenWidthF() / (float)mRenderWidth;
		auto heightFactor = GetScreenHeightF() / (float)mRenderHeight;
		mSceneScale = std::min<float>(widthFactor, heightFactor);

		// Calculate the rectangle on the back buffer where the scene will
		// be stretched to
		auto drawWidth = mSceneScale * mRenderWidth;
		auto drawHeight = mSceneScale * mRenderHeight;
		auto drawX = (GetScreenWidthF() - drawWidth) / 2;
		auto drawY = (GetScreenHeightF() - drawHeight) / 2;
		mSceneRect = XMFLOAT4(drawX, drawY, drawWidth, drawHeight);

		mSceneSurface.Release();
		mSceneDepthSurface.Release();

		auto aaType = D3DMULTISAMPLE_NONE;
		if (mAntiAliasing && !mSupportedAaSamples.empty()) {
			aaType = mSupportedAaSamples.back();
		}
		
		// Create surfaces for the scene
		D3DLOG(mDevice->CreateRenderTarget(
			mRenderWidth,
			mRenderHeight,
			mBackBufferDesc.Format,
			aaType,
			0,
			FALSE,
			&mSceneSurface,
			nullptr));

		D3DLOG(mDevice->CreateDepthStencilSurface(
			mRenderWidth,
			mRenderHeight,
			D3DFMT_D16,
			aaType,
			0,
			TRUE,
			&mSceneDepthSurface,
			nullptr));

	}

	void RenderingDevice::TakeScaledScreenshot(const std::string &filename, int width, int height, int quality) {
		logger->debug("Creating screenshot with size {}x{} in {}", width, height, filename);
		
		D3DSURFACE_DESC desc;
		mSceneSurface->GetDesc(&desc);

		// Support taking unscaled screenshots
		auto stretch = true;
		if (width == 0 || height == 0) {
			width = desc.Width;
			height = desc.Height;
			stretch = false;
		}

		// Create system memory surface to copy the screenshot to
		CComPtr<IDirect3DSurface9> sysMemSurface;
		if (D3DLOG(mDevice->CreateOffscreenPlainSurface(width, height, desc.Format, D3DPOOL_SYSTEMMEM, &sysMemSurface, nullptr)) != D3D_OK) {
			logger->error("Unable to create offscreen surface for copying the screenshot");
			return;
		}

		if (stretch || desc.MultiSampleType != D3DMULTISAMPLE_NONE) {
			/*
				Create the secondary render target without multi sampling.
			*/
			CComPtr<IDirect3DSurface9> stretchedScene;
			if (D3DLOG(mDevice->CreateRenderTarget(width, height, desc.Format, D3DMULTISAMPLE_NONE, 0, false, &stretchedScene, NULL)) != D3D_OK) {
				return;
			}

			if (D3DLOG(mDevice->StretchRect(mSceneSurface, nullptr, stretchedScene, nullptr, D3DTEXF_LINEAR)) != D3D_OK) {
				logger->error("Unable to copy front buffer to target surface for screenshot");
				return;
			}

			if (D3DLOG(mDevice->GetRenderTargetData(stretchedScene, sysMemSurface))) {
				logger->error("Unable to copy stretched render target to system memory.");
				return;
			}
		} else {
			if (D3DLOG(mDevice->GetRenderTargetData(mSceneSurface, sysMemSurface))) {
				logger->error("Unable to copy scene render target to system memory.");
				return;
			}
		}

		/*
		Get access to the pixel data for the surface and encode it to a JPEG.
		*/
		D3DLOCKED_RECT locked;
		if (D3DLOG(sysMemSurface->LockRect(&locked, nullptr, 0))) {
			logger->error("Unable to lock screenshot surface.");
			return;
		}

		// Clamp quality to [1, 100]
		quality = std::min(100, std::max(1, quality));

		auto jpegData(gfx::EncodeJpeg(reinterpret_cast<uint8_t*>(locked.pBits),
			gfx::JpegPixelFormat::BGRX,
			width,
			height,
			quality,
			locked.Pitch));

		if (D3DLOG(sysMemSurface->UnlockRect())) {
			logger->error("Unable to unlock screenshot surface.");
			return;
		}

		// We have to write using tio or else it goes god knows where
		try {
			vfs->WriteBinaryFile(filename, jpegData);
		} catch (std::exception &e) {
			logger->error("Unable to save screenshot due to an IO error: {}", e.what());

		}
	}
	
	gfx::DynamicTexturePtr RenderingDevice::CreateDynamicTexture(D3DFORMAT format, int width, int height) {

		CComPtr<IDirect3DTexture9> texture;

		if (D3DLOG(mDevice->CreateTexture(width, 
			                              height, 
										  1, 
									      D3DUSAGE_DYNAMIC, 
									      format, 
								          D3DPOOL_DEFAULT, 
								          &texture, 
			                              nullptr)) != D3D_OK) {
			return nullptr;
		}

		Size size{ width, height };
		
		return std::make_shared<DynamicTexture>(texture, size);

	}

	RenderTargetTexturePtr RenderingDevice::CreateRenderTargetTexture(D3DFORMAT format, int width, int height) {

		CComPtr<IDirect3DTexture9> texture;

		if (D3DLOG(mDevice->CreateTexture(width,
			height,
			1,
			D3DUSAGE_RENDERTARGET,
			format,
			D3DPOOL_DEFAULT,
			&texture,
			nullptr)) != D3D_OK) {
			return nullptr;
		}

		Size size{ width, height };

		return std::make_shared<RenderTargetTexture>(texture, size);

	}
	
}
