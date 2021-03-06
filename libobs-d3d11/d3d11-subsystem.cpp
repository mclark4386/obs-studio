/******************************************************************************
    Copyright (C) 2013 by Hugh Bailey <obs.jim@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include <util/base.h>
#include <util/platform.h>
#include <graphics/matrix3.h>
#include "d3d11-subsystem.hpp"

#ifdef _MSC_VER
/* alignment warning - despite the fact that alignment is already fixed */
#pragma warning (disable : 4316)
#endif

static const IID dxgiFactory2 =
{0x50c83a1c, 0xe072, 0x4c48, {0x87, 0xb0, 0x36, 0x30, 0xfa, 0x36, 0xa6, 0xd0}};

static inline void make_swap_desc(DXGI_SWAP_CHAIN_DESC &desc,
		gs_init_data *data)
{
	memset(&desc, 0, sizeof(desc));
	desc.BufferCount       = data->num_backbuffers;
	desc.BufferDesc.Format = ConvertGSTextureFormat(data->format);
	desc.BufferDesc.Width  = data->cx;
	desc.BufferDesc.Height = data->cy;
	desc.BufferUsage       = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	desc.OutputWindow      = (HWND)data->window.hwnd;
	desc.SampleDesc.Count  = 1;
	desc.Windowed          = true;
}

void gs_swap_chain::InitTarget(uint32_t cx, uint32_t cy)
{
	HRESULT hr;

	target.width  = cx;
	target.height = cy;

	hr = swap->GetBuffer(0, __uuidof(ID3D11Texture2D),
			(void**)target.texture.Assign());
	if (FAILED(hr))
		throw HRError("Failed to get swap buffer texture", hr);

	hr = device->device->CreateRenderTargetView(target.texture, NULL,
			target.renderTarget[0].Assign());
	if (FAILED(hr))
		throw HRError("Failed to create swap render target view", hr);
}

void gs_swap_chain::InitZStencilBuffer(uint32_t cx, uint32_t cy)
{
	zs.width  = cx;
	zs.height = cy;

	if (zs.format != GS_ZS_NONE && cx != 0 && cy != 0) {
		zs.InitBuffer();
	} else {
		zs.texture.Clear();
		zs.view.Clear();
	}
}

void gs_swap_chain::Resize(uint32_t cx, uint32_t cy)
{
	RECT clientRect;
	HRESULT hr;

	target.texture.Clear();
	target.renderTarget[0].Clear();
	zs.texture.Clear();
	zs.view.Clear();

	if (cx == 0 || cy == 0) {
		GetClientRect(hwnd, &clientRect);
		if (cx == 0) cx = clientRect.right;
		if (cy == 0) cy = clientRect.bottom;
	}

	hr = swap->ResizeBuffers(numBuffers, cx, cy, target.dxgiFormat, 0);
	if (FAILED(hr))
		throw HRError("Failed to resize swap buffers", hr);

	InitTarget(cx, cy);
	InitZStencilBuffer(cx, cy);
}

void gs_swap_chain::Init(gs_init_data *data)
{
	target.device         = device;
	target.isRenderTarget = true;
	target.format         = data->format;
	target.dxgiFormat     = ConvertGSTextureFormat(data->format);
	InitTarget(data->cx, data->cy);

	zs.device     = device;
	zs.format     = data->zsformat;
	zs.dxgiFormat = ConvertGSZStencilFormat(data->zsformat);
	InitZStencilBuffer(data->cx, data->cy);
}

gs_swap_chain::gs_swap_chain(gs_device *device, gs_init_data *data)
	: device     (device),
	  numBuffers (data->num_backbuffers),
	  hwnd       ((HWND)data->window.hwnd)
{
	HRESULT hr;
	DXGI_SWAP_CHAIN_DESC swapDesc;

	make_swap_desc(swapDesc, data);
	hr = device->factory->CreateSwapChain(device->device, &swapDesc,
			swap.Assign());
	if (FAILED(hr))
		throw HRError("Failed to create swap chain", hr);

	Init(data);
}

void gs_device::InitFactory(uint32_t adapterIdx, IDXGIAdapter1 **padapter)
{
	HRESULT hr;
	IID factoryIID = (GetWinVer() >= 0x602) ? dxgiFactory2 :
		__uuidof(IDXGIFactory1);

	hr = CreateDXGIFactory1(factoryIID, (void**)factory.Assign());
	if (FAILED(hr))
		throw HRError("Failed to create DXGIFactory", hr);

	hr = factory->EnumAdapters1(adapterIdx, padapter);
	if (FAILED(hr))
		throw HRError("Failed to enumerate DXGIAdapter", hr);
}

const static D3D_FEATURE_LEVEL featureLevels[] =
{
	D3D_FEATURE_LEVEL_11_0,
	D3D_FEATURE_LEVEL_10_1,
	D3D_FEATURE_LEVEL_10_0,
	D3D_FEATURE_LEVEL_9_3,
};

void gs_device::InitDevice(gs_init_data *data, IDXGIAdapter *adapter)
{
	wstring adapterName;
	DXGI_SWAP_CHAIN_DESC swapDesc;
	DXGI_ADAPTER_DESC desc;
	D3D_FEATURE_LEVEL levelUsed;
	HRESULT hr;

	make_swap_desc(swapDesc, data);

	uint32_t createFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
	//createFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	adapterName = (adapter->GetDesc(&desc) == S_OK) ? desc.Description :
		L"<unknown>";

	char *adapterNameUTF8;
	os_wcs_to_utf8(adapterName.c_str(), 0, &adapterNameUTF8);
	blog(LOG_INFO, "Loading up D3D11 on adapter %s", adapterNameUTF8);
	bfree(adapterNameUTF8);

	hr = D3D11CreateDeviceAndSwapChain(adapter, D3D_DRIVER_TYPE_UNKNOWN,
			NULL, createFlags, featureLevels,
			sizeof(featureLevels) / sizeof(D3D_FEATURE_LEVEL),
			D3D11_SDK_VERSION, &swapDesc,
			defaultSwap.swap.Assign(), device.Assign(),
			&levelUsed, context.Assign());
	if (FAILED(hr))
		throw HRError("Failed to create device and swap chain", hr);

	blog(LOG_INFO, "D3D11 loaded sucessfully, feature level used: %u",
			(uint32_t)levelUsed);

	defaultSwap.device     = this;
	defaultSwap.hwnd       = (HWND)data->window.hwnd;
	defaultSwap.numBuffers = data->num_backbuffers;
	defaultSwap.Init(data);
}

static inline void ConvertStencilSide(D3D11_DEPTH_STENCILOP_DESC &desc,
		const StencilSide &side)
{
	desc.StencilFunc        = ConvertGSDepthTest(side.test);
	desc.StencilFailOp      = ConvertGSStencilOp(side.fail);
	desc.StencilDepthFailOp = ConvertGSStencilOp(side.zfail);
	desc.StencilPassOp      = ConvertGSStencilOp(side.zpass);
}

ID3D11DepthStencilState *gs_device::AddZStencilState()
{
	HRESULT hr;
	D3D11_DEPTH_STENCIL_DESC dsd;
	SavedZStencilState savedState(zstencilState);
	ID3D11DepthStencilState *state;

	dsd.DepthEnable      = zstencilState.depthEnabled;
	dsd.DepthFunc        = ConvertGSDepthTest(zstencilState.depthFunc);
	dsd.DepthWriteMask   = zstencilState.depthWriteEnabled ?
		D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
	dsd.StencilEnable    = zstencilState.stencilEnabled;
	dsd.StencilReadMask  = D3D11_DEFAULT_STENCIL_READ_MASK;
	dsd.StencilWriteMask = zstencilState.stencilWriteEnabled ?
		D3D11_DEFAULT_STENCIL_WRITE_MASK : 0;
	ConvertStencilSide(dsd.FrontFace, zstencilState.stencilFront);
	ConvertStencilSide(dsd.BackFace,  zstencilState.stencilBack);

	hr = device->CreateDepthStencilState(&dsd, savedState.state.Assign());
	if (FAILED(hr))
		throw HRError("Failed to create depth stencil state", hr);

	state = savedState.state;
	zstencilStates.push_back(savedState);

	return state;
}

ID3D11RasterizerState *gs_device::AddRasterState()
{
	HRESULT hr;
	D3D11_RASTERIZER_DESC rd;
	SavedRasterState savedState(rasterState);
	ID3D11RasterizerState *state;

	memset(&rd, 0, sizeof(rd));
	/* use CCW to convert to a right-handed coordinate system */
	rd.FrontCounterClockwise = true;
	rd.FillMode              = D3D11_FILL_SOLID;
	rd.CullMode              = ConvertGSCullMode(rasterState.cullMode);
	rd.DepthClipEnable       = true;
	rd.ScissorEnable         = rasterState.scissorEnabled;

	hr = device->CreateRasterizerState(&rd, savedState.state.Assign());
	if (FAILED(hr))
		throw HRError("Failed to create rasterizer state", hr);

	state = savedState.state;
	rasterStates.push_back(savedState);

	return state;
}

ID3D11BlendState *gs_device::AddBlendState()
{
	HRESULT hr;
	D3D11_BLEND_DESC bd;
	SavedBlendState savedState(blendState);
	ID3D11BlendState *state;

	memset(&bd, 0, sizeof(bd));
	for (int i = 0; i < 8; i++) {
		bd.RenderTarget[i].BlendEnable    = blendState.blendEnabled;
		bd.RenderTarget[i].BlendOp        = D3D11_BLEND_OP_ADD;
		bd.RenderTarget[i].BlendOpAlpha   = D3D11_BLEND_OP_ADD;
		bd.RenderTarget[i].SrcBlendAlpha  = D3D11_BLEND_ONE;
		bd.RenderTarget[i].DestBlendAlpha = D3D11_BLEND_ZERO;
		bd.RenderTarget[i].SrcBlend =
			ConvertGSBlendType(blendState.srcFactor);
		bd.RenderTarget[i].DestBlend =
			ConvertGSBlendType(blendState.destFactor);
		bd.RenderTarget[i].RenderTargetWriteMask =
			D3D11_COLOR_WRITE_ENABLE_ALL;
	}

	hr = device->CreateBlendState(&bd, savedState.state.Assign());
	if (FAILED(hr))
		throw HRError("Failed to create disabled blend state", hr);

	state = savedState.state;
	blendStates.push_back(savedState);

	return state;
}

void gs_device::UpdateZStencilState()
{
	ID3D11DepthStencilState *state = NULL;

	if (!zstencilStateChanged)
		return;

	for (size_t i = 0; i < zstencilStates.size(); i++) {
		SavedZStencilState &s = zstencilStates[i];
		if (memcmp(&s, &zstencilState, sizeof(zstencilState)) == 0) {
			state = s.state;
			break;
		}
	}

	if (!state)
		state = AddZStencilState();

	if (state != curDepthStencilState) {
		context->OMSetDepthStencilState(state, 0);
		curDepthStencilState = state;
	}

	zstencilStateChanged = false;
}

void gs_device::UpdateRasterState()
{
	ID3D11RasterizerState *state = NULL;

	if (!rasterStateChanged)
		return;

	for (size_t i = 0; i < rasterStates.size(); i++) {
		SavedRasterState &s = rasterStates[i];
		if (memcmp(&s, &rasterState, sizeof(rasterState)) == 0) {
			state = s.state;
			break;
		}
	}

	if (!state)
		state = AddRasterState();

	if (state != curRasterState) {
		context->RSSetState(state);
		curRasterState = state;
	}

	rasterStateChanged = false;
}

void gs_device::UpdateBlendState()
{
	ID3D11BlendState *state = NULL;

	if (!blendStateChanged)
		return;

	for (size_t i = 0; i < blendStates.size(); i++) {
		SavedBlendState &s = blendStates[i];
		if (memcmp(&s, &blendState, sizeof(blendState)) == 0) {
			state = s.state;
			break;
		}
	}

	if (!state)
		state = AddBlendState();

	if (state != curBlendState) {
		float f[4] = {1.0f, 1.0f, 1.0f, 1.0f};
		context->OMSetBlendState(state, f, 0xFFFFFFFF);
		curBlendState = state;
	}

	blendStateChanged = false;
}

void gs_device::UpdateViewProjMatrix()
{
	matrix3 cur_matrix;
	gs_matrix_get(&cur_matrix);

	matrix4_from_matrix3(&curViewMatrix, &cur_matrix);

	/* negate Z col of the view matrix for right-handed coordinate system */
	curViewMatrix.x.z = -curViewMatrix.x.z;
	curViewMatrix.y.z = -curViewMatrix.y.z;
	curViewMatrix.z.z = -curViewMatrix.z.z;
	curViewMatrix.t.z = -curViewMatrix.t.z;

	matrix4_mul(&curViewProjMatrix, &curViewMatrix, &curProjMatrix);
	matrix4_transpose(&curViewProjMatrix, &curViewProjMatrix);

	if (curVertexShader->viewProj)
		shader_setmatrix4(curVertexShader, curVertexShader->viewProj,
				&curViewProjMatrix);
}

gs_device::gs_device(gs_init_data *data)
	: curRenderTarget      (NULL),
	  curZStencilBuffer    (NULL),
	  curRenderSide        (0),
	  curIndexBuffer       (NULL),
	  curVertexBuffer      (NULL),
	  curVertexShader      (NULL),
	  curPixelShader       (NULL),
	  curSwapChain         (&defaultSwap),
	  zstencilStateChanged (true),
	  rasterStateChanged   (true),
	  blendStateChanged    (true),
	  curDepthStencilState (NULL),
	  curRasterState       (NULL),
	  curBlendState        (NULL),
	  curToplogy           (D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED)
{
	ComPtr<IDXGIAdapter1> adapter;

	matrix4_identity(&curProjMatrix);
	matrix4_identity(&curViewMatrix);
	matrix4_identity(&curViewProjMatrix);

	memset(&viewport, 0, sizeof(viewport));

	for (size_t i = 0; i < GS_MAX_TEXTURES; i++) {
		curTextures[i] = NULL;
		curSamplers[i] = NULL;
	}

	InitFactory(data->adapter, adapter.Assign());
	InitDevice(data, adapter);
	device_setrendertarget(this, NULL, NULL);
}

const char *device_preprocessor_name(void)
{
	return "_D3D11";
}

gs_device *device_create(gs_init_data *data)
{
	gs_device *device = NULL;

	try {
		device = new gs_device(data);
	} catch (HRError error) {
		blog(LOG_ERROR, "device_create (D3D11): %s (%08lX)", error.str,
				error.hr);
	}

	return device;
}

void device_destroy(device_t device)
{
	delete device;
}

void device_entercontext(device_t device)
{
	/* does nothing */
}

void device_leavecontext(device_t device)
{
	/* does nothing */
}

swapchain_t device_create_swapchain(device_t device, struct gs_init_data *data)
{
	gs_swap_chain *swap = NULL;

	try {
		swap = new gs_swap_chain(device, data);
	} catch (HRError error) {
		blog(LOG_ERROR, "device_create_swapchain (D3D11): %s (%08lX)",
				error.str, error.hr);
	}

	return swap;
}

void device_resize(device_t device, uint32_t cx, uint32_t cy)
{
	try {
		ID3D11RenderTargetView *renderView = NULL;
		ID3D11DepthStencilView *depthView  = NULL;
		int i = device->curRenderSide;

		device->context->OMSetRenderTargets(1, &renderView, depthView);
		device->curSwapChain->Resize(cx, cy);

		if (device->curRenderTarget)
			renderView = device->curRenderTarget->renderTarget[i];
		if (device->curZStencilBuffer)
			depthView  = device->curZStencilBuffer->view;
		device->context->OMSetRenderTargets(1, &renderView, depthView);

	} catch (HRError error) {
		blog(LOG_ERROR, "device_resize (D3D11): %s (%08lX)",
				error.str, error.hr);
	}
}

void device_getsize(device_t device, uint32_t *cx, uint32_t *cy)
{
	*cx = device->curSwapChain->target.width;
	*cy = device->curSwapChain->target.height;
}

uint32_t device_getwidth(device_t device)
{
	return device->curSwapChain->target.width;
}

uint32_t device_getheight(device_t device)
{
	return device->curSwapChain->target.height;
}

texture_t device_create_texture(device_t device, uint32_t width,
		uint32_t height, enum gs_color_format color_format,
		uint32_t levels, const void **data, uint32_t flags)
{
	gs_texture *texture = NULL;
	try {
		texture = new gs_texture_2d(device, width, height, color_format,
				levels, data, flags, GS_TEXTURE_2D, false,
				false);
	} catch (HRError error) {
		blog(LOG_ERROR, "device_create_texture (D3D11): %s (%08lX)",
				error.str, error.hr);
	} catch (const char *error) {
		blog(LOG_ERROR, "device_create_texture (D3D11): %s", error);
	}

	return texture;
}

texture_t device_create_cubetexture(device_t device, uint32_t size,
		enum gs_color_format color_format, uint32_t levels,
		const void **data, uint32_t flags)
{
	gs_texture *texture = NULL;
	try {
		texture = new gs_texture_2d(device, size, size, color_format,
				levels, data, flags, GS_TEXTURE_CUBE, false,
				false);
	} catch (HRError error) {
		blog(LOG_ERROR, "device_create_cubetexture (D3D11): %s "
		                "(%08lX)",
		                error.str, error.hr);
	} catch (const char *error) {
		blog(LOG_ERROR, "device_create_cubetexture (D3D11): %s",
				error);
	}

	return texture;
}

texture_t device_create_volumetexture(device_t device, uint32_t width,
		uint32_t height, uint32_t depth,
		enum gs_color_format color_format, uint32_t levels,
		const void **data, uint32_t flags)
{
	/* TODO */
	return NULL;
}

zstencil_t device_create_zstencil(device_t device, uint32_t width,
		uint32_t height, enum gs_zstencil_format format)
{
	gs_zstencil_buffer *zstencil = NULL;
	try {
		zstencil = new gs_zstencil_buffer(device, width, height,
				format);
	} catch (HRError error) {
		blog(LOG_ERROR, "device_create_zstencil (D3D11): %s (%08lX)",
				error.str, error.hr);
	}

	return zstencil;
}

stagesurf_t device_create_stagesurface(device_t device, uint32_t width,
		uint32_t height, enum gs_color_format color_format)
{
	gs_stage_surface *surf = NULL;
	try {
		surf = new gs_stage_surface(device, width, height,
				color_format);
	} catch (HRError error) {
		blog(LOG_ERROR, "device_create_stagesurface (D3D11): %s "
		                "(%08lX)",
				error.str, error.hr);
	}

	return surf;
}

samplerstate_t device_create_samplerstate(device_t device,
		struct gs_sampler_info *info)
{
	gs_sampler_state *ss = NULL;
	try {
		ss = new gs_sampler_state(device, info);
	} catch (HRError error) {
		blog(LOG_ERROR, "device_create_samplerstate (D3D11): %s "
		                "(%08lX)",
				error.str, error.hr);
	}

	return ss;
}

shader_t device_create_vertexshader(device_t device,
		const char *shader_string, const char *file,
		char **error_string)
{
	gs_vertex_shader *shader = NULL;
	try {
		shader = new gs_vertex_shader(device, file, shader_string);

	} catch (HRError error) {
		blog(LOG_ERROR, "device_create_vertexshader (D3D11): %s "
		                "(%08lX)",
				error.str, error.hr);

	} catch (ShaderError error) {
		const char *buf = (const char*)error.errors->GetBufferPointer();
		if (error_string)
			*error_string = bstrdup(buf);
		blog(LOG_ERROR, "device_create_vertexshader (D3D11): "
		                "Compile warnings/errors for %s:\n%s",
		                file, buf);

	} catch (const char *error) {
		blog(LOG_ERROR, "device_create_vertexshader (D3D11): %s",
				error);
	}

	return shader;
}

shader_t device_create_pixelshader(device_t device,
		const char *shader_string, const char *file,
		char **error_string)
{
	gs_pixel_shader *shader = NULL;
	try {
		shader = new gs_pixel_shader(device, file, shader_string);

	} catch (HRError error) {
		blog(LOG_ERROR, "device_create_pixelshader (D3D11): %s "
		                "(%08lX)",
				error.str, error.hr);

	} catch (ShaderError error) {
		const char *buf = (const char*)error.errors->GetBufferPointer();
		if (error_string)
			*error_string = bstrdup(buf);
		blog(LOG_ERROR, "device_create_pixelshader (D3D11): "
		                "Compiler warnings/errors for %s:\n%s",
		                file, buf);

	} catch (const char *error) {
		blog(LOG_ERROR, "device_create_pixelshader (D3D11): %s",
				error);
	}

	return shader;
}

vertbuffer_t device_create_vertexbuffer(device_t device,
		struct vb_data *data, uint32_t flags)
{
	gs_vertex_buffer *buffer = NULL;
	try {
		buffer = new gs_vertex_buffer(device, data, flags);
	} catch (HRError error) {
		blog(LOG_ERROR, "device_create_vertexbuffer (D3D11): %s "
		                "(%08lX)",
				error.str, error.hr);
	} catch (const char *error) {
		blog(LOG_ERROR, "device_create_vertexbuffer (D3D11): %s",
				error);
	}

	return buffer;
}

indexbuffer_t device_create_indexbuffer(device_t device,
		enum gs_index_type type, void *indices, size_t num,
		uint32_t flags)
{
	gs_index_buffer *buffer = NULL;
	try {
		buffer = new gs_index_buffer(device, type, indices, num, flags);
	} catch (HRError error) {
		blog(LOG_ERROR, "device_create_indexbuffer (D3D11): %s (%08lX)",
				error.str, error.hr);
	}

	return buffer;
}

enum gs_texture_type device_gettexturetype(texture_t texture)
{
	return texture->type;
}

void device_load_vertexbuffer(device_t device, vertbuffer_t vertbuffer)
{
	if (device->curVertexBuffer == vertbuffer)
		return;

	device->curVertexBuffer = vertbuffer;

	if (!device->curVertexShader)
		return;

	vector<ID3D11Buffer*> buffers;
	vector<uint32_t> strides;
	vector<uint32_t> offsets;

	if (vertbuffer) {
		vertbuffer->MakeBufferList(device->curVertexShader,
				buffers, strides);
	} else {
		size_t buffersToClear =
			device->curVertexShader->NumBuffersExpected();
		buffers.resize(buffersToClear);
		strides.resize(buffersToClear);
	}

	offsets.resize(buffers.size());
	device->context->IASetVertexBuffers(0, (UINT)buffers.size(),
			buffers.data(), strides.data(), offsets.data());
}

void device_load_indexbuffer(device_t device, indexbuffer_t indexbuffer)
{
	DXGI_FORMAT format;
	ID3D11Buffer *buffer;

	if (device->curIndexBuffer == indexbuffer)
		return;

	if (indexbuffer) {
		switch (indexbuffer->indexSize) {
		case 2: format = DXGI_FORMAT_R16_UINT; break;
		case 4: format = DXGI_FORMAT_R32_UINT; break;
		}

		buffer = indexbuffer->indexBuffer;
	} else {
		buffer = NULL;
		format = DXGI_FORMAT_R32_UINT;
	}

	device->curIndexBuffer = indexbuffer;
	device->context->IASetIndexBuffer(buffer, format, 0);
}

void device_load_texture(device_t device, texture_t tex, int unit)
{
	ID3D11ShaderResourceView *view = NULL;

	if (device->curTextures[unit] == tex)
		return;

	if (tex)
		view = tex->shaderRes;

	device->curTextures[unit] = tex;
	device->context->PSSetShaderResources(unit, 1, &view);
}

void device_load_samplerstate(device_t device,
		samplerstate_t samplerstate, int unit)
{
	ID3D11SamplerState *state = NULL;

	if (device->curSamplers[unit] == samplerstate)
		return;

	if (samplerstate)
		state = samplerstate->state;

	device->curSamplers[unit] = samplerstate;
	device->context->PSSetSamplers(unit, 1, &state);
}

void device_load_vertexshader(device_t device, shader_t vertshader)
{
	ID3D11VertexShader *shader    = NULL;
	ID3D11InputLayout  *layout    = NULL;
	ID3D11Buffer       *constants = NULL;

	if (device->curVertexShader == vertshader)
		return;

	gs_vertex_shader *vs = static_cast<gs_vertex_shader*>(vertshader);
	gs_vertex_buffer *curVB = device->curVertexBuffer;

	if (vertshader) {
		if (vertshader->type != SHADER_VERTEX) {
			blog(LOG_ERROR, "device_load_vertexshader (D3D11): "
			                "Specified shader is not a vertex "
			                "shader");
			return;
		}

		if (curVB)
			device_load_vertexbuffer(device, NULL);

		shader    = vs->shader;
		layout    = vs->layout;
		constants = vs->constants;
	}

	device->curVertexShader = vs;
	device->context->VSSetShader(shader, NULL, 0);
	device->context->IASetInputLayout(layout);
	device->context->VSSetConstantBuffers(0, 1, &constants);

	if (vertshader && curVB)
		device_load_vertexbuffer(device, curVB);
}

static inline void clear_textures(device_t device)
{
	ID3D11ShaderResourceView *views[GS_MAX_TEXTURES];
	memset(views,               0, sizeof(views));
	memset(device->curTextures, 0, sizeof(device->curTextures));
	device->context->PSSetShaderResources(0, GS_MAX_TEXTURES, views);
}

void device_load_pixelshader(device_t device, shader_t pixelshader)
{
	ID3D11PixelShader  *shader    = NULL;
	ID3D11Buffer       *constants = NULL;
	ID3D11SamplerState *states[GS_MAX_TEXTURES];

	if (device->curPixelShader == pixelshader)
		return;

	gs_pixel_shader *ps = static_cast<gs_pixel_shader*>(pixelshader);

	if (pixelshader) {
		if (pixelshader->type != SHADER_PIXEL) {
			blog(LOG_ERROR, "device_load_pixelshader (D3D11): "
			                "Specified shader is not a pixel "
			                "shader");
			return;
		}

		shader    = ps->shader;
		constants = ps->constants;
		ps->GetSamplerStates(states);
	} else {
		memset(states, 0, sizeof(states));
	}

	clear_textures(device);

	device->curPixelShader = ps;
	device->context->PSSetShader(shader, NULL, 0);
	device->context->PSSetConstantBuffers(0, 1, &constants);
	device->context->PSSetSamplers(0, GS_MAX_TEXTURES, states);
}

void device_load_defaultsamplerstate(device_t device, bool b_3d, int unit)
{
	/* TODO */
}

shader_t device_getvertexshader(device_t device)
{
	return device->curVertexShader;
}

shader_t device_getpixelshader(device_t device)
{
	return device->curPixelShader;
}

texture_t device_getrendertarget(device_t device)
{
	if (device->curRenderTarget == &device->curSwapChain->target)
		return NULL;

	return device->curRenderTarget;
}

zstencil_t device_getzstenciltarget(device_t device)
{
	if (device->curZStencilBuffer == &device->curSwapChain->zs)
		return NULL;

	return device->curZStencilBuffer;
}

void device_setrendertarget(device_t device, texture_t tex, zstencil_t zstencil)
{
	if (!tex)
		tex = &device->curSwapChain->target;
	if (!zstencil)
		zstencil = &device->curSwapChain->zs;

	if (device->curRenderTarget   == tex &&
	    device->curZStencilBuffer == zstencil)
		return;

	if (tex->type != GS_TEXTURE_2D) {
		blog(LOG_ERROR, "device_setrendertarget (D3D11): "
		                "texture is not a 2D texture");
		return;
	}

	gs_texture_2d *tex2d = static_cast<gs_texture_2d*>(tex);
	if (!tex2d->renderTarget[0]) {
		blog(LOG_ERROR, "device_setrendertarget (D3D11): "
		                "texture is not a render target");
		return;
	}

	ID3D11RenderTargetView *rt = tex2d->renderTarget[0];

	device->curRenderTarget   = tex2d;
	device->curRenderSide     = 0;
	device->curZStencilBuffer = zstencil;
	device->context->OMSetRenderTargets(1, &rt, zstencil->view);
}

void device_setcuberendertarget(device_t device, texture_t tex, int side,
		zstencil_t zstencil)
{
	if (!tex) {
		tex = &device->curSwapChain->target;
		side = 0;
	}

	if (!zstencil)
		zstencil = &device->curSwapChain->zs;

	if (device->curRenderTarget   == tex  &&
	    device->curRenderSide     == side &&
	    device->curZStencilBuffer == zstencil)
		return;

	if (tex->type != GS_TEXTURE_CUBE) {
		blog(LOG_ERROR, "device_setcuberendertarget (D3D11): "
		                "texture is not a cube texture");
		return;
	}

	gs_texture_2d *tex2d = static_cast<gs_texture_2d*>(tex);
	if (!tex2d->renderTarget[side]) {
		blog(LOG_ERROR, "device_setcuberendertarget (D3D11): "
				"texture is not a render target");
		return;
	}

	ID3D11RenderTargetView *rt = tex2d->renderTarget[0];

	device->curRenderTarget   = tex2d;
	device->curRenderSide     = side;
	device->curZStencilBuffer = zstencil;
	device->context->OMSetRenderTargets(1, &rt, zstencil->view);
}

inline void gs_device::CopyTex(ID3D11Texture2D *dst, texture_t src)
{
	if (src->type != GS_TEXTURE_2D)
		throw "Source texture must be a 2D texture";

	gs_texture_2d *tex2d = static_cast<gs_texture_2d*>(src);
	context->CopyResource(dst, tex2d->texture);
}

void device_copy_texture(device_t device, texture_t dst, texture_t src)
{
	try {
		gs_texture_2d *src2d = static_cast<gs_texture_2d*>(src);
		gs_texture_2d *dst2d = static_cast<gs_texture_2d*>(dst);

		if (!src)
			throw "Source texture is NULL";
		if (!dst)
			throw "Destination texture is NULL";
		if (src->type != GS_TEXTURE_2D || dst->type != GS_TEXTURE_2D)
			throw "Source and destination textures must be a 2D "
			      "textures";
		if (dst->format != src->format)
			throw "Source and destination formats do not match";
		if (dst2d->width  != src2d->width ||
		    dst2d->height != src2d->height)
			throw "Source and destination must have the same "
			      "dimensions";

		gs_texture_2d *tex2d = static_cast<gs_texture_2d*>(dst);
		device->CopyTex(tex2d->texture, src);

	} catch (const char *error) {
		blog(LOG_ERROR, "device_copy_texture (D3D11): %s", error);
	}
}

void device_stage_texture(device_t device, stagesurf_t dst, texture_t src)
{
	try {
		gs_texture_2d *src2d = static_cast<gs_texture_2d*>(src);

		if (!src)
			throw "Source texture is NULL";
		if (src->type != GS_TEXTURE_2D)
			throw "Source texture must be a 2D texture";
		if (!dst)
			throw "Destination surface is NULL";
		if (dst->format != src->format)
			throw "Source and destination formats do not match";
		if (dst->width  != src2d->width ||
		    dst->height != src2d->height)
			throw "Source and destination must have the same "
			      "dimensions";

		device->CopyTex(dst->texture, src);

	} catch (const char *error) {
		blog(LOG_ERROR, "device_copy_texture (D3D11): %s", error);
	}
}

void device_beginscene(device_t device)
{
	clear_textures(device);
}

void device_draw(device_t device, enum gs_draw_mode draw_mode,
		uint32_t start_vert, uint32_t num_verts)
{
	try {
		if (!device->curVertexShader)
			throw "No vertex shader specified";

		if (!device->curPixelShader)
			throw "No pixel shader specified";

		if (!device->curVertexBuffer)
			throw "No vertex buffer specified";

		effect_t effect = gs_geteffect();
		if (effect)
			effect_updateparams(effect);

		device->UpdateBlendState();
		device->UpdateRasterState();
		device->UpdateZStencilState();
		device->UpdateViewProjMatrix();
		device->curVertexShader->UploadParams();
		device->curPixelShader->UploadParams();

	} catch (const char *error) {
		blog(LOG_ERROR, "device_draw (D3D11): %s", error);
		return;

	} catch (HRError error) {
		blog(LOG_ERROR, "device_draw (D3D11): %s (%08lX)", error.str,
				error.hr);
		return;
	}

	D3D11_PRIMITIVE_TOPOLOGY newTopology = ConvertGSTopology(draw_mode);
	if (device->curToplogy != newTopology) {
		device->context->IASetPrimitiveTopology(newTopology);
		device->curToplogy = newTopology;
	}

	if (device->curIndexBuffer) {
		if (num_verts == 0)
			num_verts = (uint32_t)device->curIndexBuffer->num;
		device->context->DrawIndexed(num_verts, start_vert, 0);
	} else {
		if (num_verts == 0)
			num_verts = (uint32_t)device->curVertexBuffer->numVerts;
		device->context->Draw(num_verts, start_vert);
	}
}

void device_endscene(device_t device)
{
	/* does nothing in D3D11 */
}

void device_load_swapchain(device_t device, swapchain_t swapchain)
{
	texture_t  target = device->curRenderTarget;
	zstencil_t zs     = device->curZStencilBuffer;
	bool is_cube = device->curRenderTarget->type == GS_TEXTURE_CUBE;

	if (target == &device->curSwapChain->target)
		target = NULL;
	if (zs == &device->curSwapChain->zs)
		zs = NULL;

	if (swapchain == NULL)
		swapchain = &device->defaultSwap;

	device->curSwapChain = swapchain;

	if (is_cube)
		device_setcuberendertarget(device, target,
				device->curRenderSide, zs);
	else
		device_setrendertarget(device, target, zs);
}

void device_clear(device_t device, uint32_t clear_flags, struct vec4 *color,
		float depth, uint8_t stencil)
{
	int side = device->curRenderSide;
	if ((clear_flags & GS_CLEAR_COLOR) != 0 && device->curRenderTarget)
		device->context->ClearRenderTargetView(
				device->curRenderTarget->renderTarget[side],
				color->ptr);

	if (device->curZStencilBuffer) {
		uint32_t flags = 0;
		if ((clear_flags & GS_CLEAR_DEPTH) != 0)
			flags |= D3D11_CLEAR_DEPTH;
		if ((clear_flags & GS_CLEAR_STENCIL) != 0)
			flags |= D3D11_CLEAR_STENCIL;

		if (flags && device->curZStencilBuffer->view)
			device->context->ClearDepthStencilView(
					device->curZStencilBuffer->view,
					flags, depth, stencil);
	}
}

void device_present(device_t device)
{
	device->curSwapChain->swap->Present(0, 0);
}

void device_setcullmode(device_t device, enum gs_cull_mode mode)
{
	if (mode == device->rasterState.cullMode)
		return;

	device->rasterState.cullMode = mode;
	device->rasterStateChanged = true;
}

enum gs_cull_mode device_getcullmode(device_t device)
{
	return device->rasterState.cullMode;
}

void device_enable_blending(device_t device, bool enable)
{
	if (enable == device->blendState.blendEnabled)
		return;

	device->blendState.blendEnabled = enable;
	device->blendStateChanged = true;
}

void device_enable_depthtest(device_t device, bool enable)
{
	if (enable == device->zstencilState.depthEnabled)
		return;

	device->zstencilState.depthEnabled = enable;
	device->zstencilStateChanged = true;
}

void device_enable_stenciltest(device_t device, bool enable)
{
	if (enable == device->zstencilState.stencilEnabled)
		return;

	device->zstencilState.stencilEnabled = enable;
	device->zstencilStateChanged = true;
}

void device_enable_stencilwrite(device_t device, bool enable)
{
	if (enable == device->zstencilState.stencilWriteEnabled)
		return;

	device->zstencilState.stencilWriteEnabled = enable;
	device->zstencilStateChanged = true;
}

void device_enable_color(device_t device, bool red, bool green,
		bool blue, bool alpha)
{
	if (device->blendState.redEnabled   == red   &&
	    device->blendState.greenEnabled == green &&
	    device->blendState.blueEnabled  == blue  &&
	    device->blendState.alphaEnabled == alpha)
		return;

	device->blendState.redEnabled   = red;
	device->blendState.greenEnabled = green;
	device->blendState.blueEnabled  = blue;
	device->blendState.alphaEnabled = alpha;
	device->blendStateChanged       = true;
}

void device_blendfunction(device_t device, enum gs_blend_type src,
		enum gs_blend_type dest)
{
	if (device->blendState.srcFactor  == src &&
	    device->blendState.destFactor == dest)
		return;

	device->blendState.srcFactor  = src;
	device->blendState.destFactor = dest;
	device->blendStateChanged     = true;
}

void device_depthfunction(device_t device, enum gs_depth_test test)
{
	if (device->zstencilState.depthFunc == test)
		return;

	device->zstencilState.depthFunc = test;
	device->zstencilStateChanged    = true;
}

static inline void update_stencilside_test(device_t device, StencilSide &side,
		gs_depth_test test)
{
	if (side.test == test)
		return;

	side.test = test;
	device->zstencilStateChanged = true;
}

void device_stencilfunction(device_t device, enum gs_stencil_side side,
		enum gs_depth_test test)
{
	int sideVal = (int)side;

	if (sideVal & GS_STENCIL_FRONT)
		update_stencilside_test(device,
				device->zstencilState.stencilFront, test);
	if (sideVal & GS_STENCIL_BACK)
		update_stencilside_test(device,
				device->zstencilState.stencilBack, test);
}

static inline void update_stencilside_op(device_t device, StencilSide &side,
		enum gs_stencil_op fail, enum gs_stencil_op zfail,
		enum gs_stencil_op zpass)
{
	if (side.fail == fail && side.zfail == zfail && side.zpass == zpass)
		return;

	side.fail  = fail;
	side.zfail = zfail;
	side.zpass = zpass;
	device->zstencilStateChanged = true;
}

void device_stencilop(device_t device, enum gs_stencil_side side,
		enum gs_stencil_op fail, enum gs_stencil_op zfail,
		enum gs_stencil_op zpass)
{
	int sideVal = (int)side;

	if (sideVal & GS_STENCIL_FRONT)
		update_stencilside_op(device,
				device->zstencilState.stencilFront,
				fail, zfail, zpass);
	if (sideVal & GS_STENCIL_BACK)
		update_stencilside_op(device,
				device->zstencilState.stencilBack,
				fail, zfail, zpass);
}

void device_enable_fullscreen(device_t device, bool enable)
{
	/* TODO */
}

int device_fullscreen_enabled(device_t device)
{
	/* TODO */
	return 0;
}

void device_setdisplaymode(device_t device,
		const struct gs_display_mode *mode)
{
	/* TODO */
}

void device_getdisplaymode(device_t device,
		struct gs_display_mode *mode)
{
	/* TODO */
}

void device_setcolorramp(device_t device, float gamma, float brightness,
		float contrast)
{
	/* TODO */
}

void device_setviewport(device_t device, int x, int y, int width,
		int height)
{
	D3D11_VIEWPORT vp;
	memset(&vp, 0, sizeof(vp));
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = (float)x;
	vp.TopLeftY = (float)y;
	vp.Width    = (float)width;
	vp.Height   = (float)height;
	device->context->RSSetViewports(1, &vp);

	device->viewport.x  = x;
	device->viewport.y  = y;
	device->viewport.cx = width;
	device->viewport.cy = height;
}

void device_getviewport(device_t device, struct gs_rect *rect)
{
	memcpy(rect, &device->viewport, sizeof(gs_rect));
}

void device_setscissorrect(device_t device, struct gs_rect *rect)
{
	D3D11_RECT d3drect;
	d3drect.left   = rect->x;
	d3drect.top    = rect->y;
	d3drect.right  = rect->x + rect->cx;
	d3drect.bottom = rect->y + rect->cy;
	device->context->RSSetScissorRects(1, &d3drect);
}

void device_ortho(device_t device, float left, float right, float top,
		float bottom, float zNear, float zFar)
{
	matrix4 *dst = &device->curProjMatrix;

	float rml = right-left;
	float bmt = bottom-top;
	float fmn = zFar-zNear;

	vec4_zero(&dst->x);
	vec4_zero(&dst->y);
	vec4_zero(&dst->z);
	vec4_zero(&dst->t);

	dst->x.x =         2.0f /  rml;
	dst->t.x = (left+right) / -rml;

	dst->y.y =         2.0f / -bmt;
	dst->t.y = (bottom+top) /  bmt;

	dst->z.z =         1.0f /  fmn;
	dst->t.z =        zNear / -fmn;

	dst->t.w = 1.0f;
}

void device_frustum(device_t device, float left, float right, float top,
		float bottom, float zNear, float zFar)
{
	matrix4 *dst = &device->curProjMatrix;

	float rml    = right-left;
	float bmt    = bottom-top;
	float fmn    = zFar-zNear;
	float nearx2 = 2.0f*zNear;

	vec4_zero(&dst->x);
	vec4_zero(&dst->y);
	vec4_zero(&dst->z);
	vec4_zero(&dst->t);

	dst->x.x =       nearx2 /  rml;
	dst->z.x = (left+right) / -rml;

	dst->y.y =       nearx2 / -bmt;
	dst->z.y = (bottom+top) /  bmt;

	dst->z.z =         zFar /  fmn;
	dst->t.z = (zNear*zFar) / -fmn;

	dst->z.w = 1.0f;
}

void device_projection_push(device_t device)
{
	mat4float mat;
	memcpy(&mat, &device->curProjMatrix, sizeof(matrix4));
	device->projStack.push_back(mat);
}

void device_projection_pop(device_t device)
{
	if (!device->projStack.size())
		return;

	mat4float *mat = device->projStack.data();
	size_t end = device->projStack.size()-1;

	/* XXX - does anyone know a better way of doing this? */
	memcpy(&device->curProjMatrix, mat+end, sizeof(matrix4));
	device->projStack.pop_back();
}

void swapchain_destroy(swapchain_t swapchain)
{
	if (!swapchain)
		return;

	gs_device *device = swapchain->device;
	if (device->curSwapChain == swapchain)
		device->curSwapChain = &device->defaultSwap;

	delete swapchain;
}

void texture_destroy(texture_t tex)
{
	delete tex;
}

uint32_t texture_getwidth(texture_t tex)
{
	if (tex->type != GS_TEXTURE_2D)
		return 0;

	return static_cast<gs_texture_2d*>(tex)->width;
}

uint32_t texture_getheight(texture_t tex)
{
	if (tex->type != GS_TEXTURE_2D)
		return 0;

	return static_cast<gs_texture_2d*>(tex)->height;
}

enum gs_color_format texture_getcolorformat(texture_t tex)
{
	if (tex->type != GS_TEXTURE_2D)
		return GS_UNKNOWN;

	return static_cast<gs_texture_2d*>(tex)->format;
}

bool texture_map(texture_t tex, void **ptr, uint32_t *linesize)
{
	HRESULT hr;

	if (tex->type != GS_TEXTURE_2D)
		return false;

	gs_texture_2d *tex2d = static_cast<gs_texture_2d*>(tex);

	D3D11_MAPPED_SUBRESOURCE map;
	hr = tex2d->device->context->Map(tex2d->texture, 0,
			D3D11_MAP_WRITE_DISCARD, 0, &map);
	if (FAILED(hr))
		return false;

	*ptr = map.pData;
	*linesize = map.RowPitch;
	return true;
}

void texture_unmap(texture_t tex)
{
	if (tex->type != GS_TEXTURE_2D)
		return;

	gs_texture_2d *tex2d = static_cast<gs_texture_2d*>(tex);
	tex2d->device->context->Unmap(tex2d->texture, 0);
}


void cubetexture_destroy(texture_t cubetex)
{
	delete cubetex;
}

uint32_t cubetexture_getsize(texture_t cubetex)
{
	if (cubetex->type != GS_TEXTURE_CUBE)
		return 0;

	gs_texture_2d *tex = static_cast<gs_texture_2d*>(cubetex);
	return tex->width;
}

enum gs_color_format cubetexture_getcolorformat(texture_t cubetex)
{
	if (cubetex->type != GS_TEXTURE_CUBE)
		return GS_UNKNOWN;

	gs_texture_2d *tex = static_cast<gs_texture_2d*>(cubetex);
	return tex->format;
}


void volumetexture_destroy(texture_t voltex)
{
	delete voltex;
}

uint32_t volumetexture_getwidth(texture_t voltex)
{
	/* TODO */
	return 0;
}

uint32_t volumetexture_getheight(texture_t voltex)
{
	/* TODO */
	return 0;
}

uint32_t volumetexture_getdepth(texture_t voltex)
{
	/* TODO */
	return 0;
}

enum gs_color_format volumetexture_getcolorformat(texture_t voltex)
{
	/* TODO */
	return GS_UNKNOWN;
}


void stagesurface_destroy(stagesurf_t stagesurf)
{
	delete stagesurf;
}

uint32_t stagesurface_getwidth(stagesurf_t stagesurf)
{
	return stagesurf->width;
}

uint32_t stagesurface_getheight(stagesurf_t stagesurf)
{
	return stagesurf->height;
}

enum gs_color_format stagesurface_getcolorformat(stagesurf_t stagesurf)
{
	return stagesurf->format;
}

bool stagesurface_map(stagesurf_t stagesurf, const uint8_t **data,
		uint32_t *linesize)
{
	D3D11_MAPPED_SUBRESOURCE map;
	if (FAILED(stagesurf->device->context->Map(stagesurf->texture, 0,
			D3D11_MAP_READ, 0, &map)))
		return false;

	*data = (uint8_t*)map.pData;
	*linesize = map.RowPitch;
	return true;
}

void stagesurface_unmap(stagesurf_t stagesurf)
{
	stagesurf->device->context->Unmap(stagesurf->texture, 0);
}


void zstencil_destroy(zstencil_t zstencil)
{
	delete zstencil;
}


void samplerstate_destroy(samplerstate_t samplerstate)
{
	delete samplerstate;
}


void vertexbuffer_destroy(vertbuffer_t vertbuffer)
{
	delete vertbuffer;
}

void vertexbuffer_flush(vertbuffer_t vertbuffer, bool rebuild)
{
	if (!vertbuffer->dynamic) {
		blog(LOG_WARNING, "vertexbuffer_flush: vertex buffer is "
		                  "not dynamic");
		return;
	}

	vertbuffer->FlushBuffer(vertbuffer->vertexBuffer,
			vertbuffer->vbd.data->points, sizeof(vec3));

	if (vertbuffer->normalBuffer)
		vertbuffer->FlushBuffer(vertbuffer->normalBuffer,
				vertbuffer->vbd.data->normals, sizeof(vec3));

	if (vertbuffer->tangentBuffer)
		vertbuffer->FlushBuffer(vertbuffer->tangentBuffer,
				vertbuffer->vbd.data->tangents, sizeof(vec3));

	if (vertbuffer->colorBuffer)
		vertbuffer->FlushBuffer(vertbuffer->colorBuffer,
				vertbuffer->vbd.data->colors, sizeof(uint32_t));

	for (size_t i = 0; i < vertbuffer->uvBuffers.size(); i++) {
		tvertarray &tv = vertbuffer->vbd.data->tvarray[i];
		vertbuffer->FlushBuffer(vertbuffer->uvBuffers[i],
				tv.array, tv.width*sizeof(float));
	}
}

struct vb_data *vertexbuffer_getdata(vertbuffer_t vertbuffer)
{
	return vertbuffer->vbd.data;
}


void indexbuffer_destroy(indexbuffer_t indexbuffer)
{
	delete indexbuffer;
}

void indexbuffer_flush(indexbuffer_t indexbuffer)
{
	HRESULT hr;

	if (!indexbuffer->dynamic)
		return;

	D3D11_MAPPED_SUBRESOURCE map;
	hr = indexbuffer->device->context->Map(indexbuffer->indexBuffer, 0,
			D3D11_MAP_WRITE_DISCARD, 0, &map);
	if (FAILED(hr))
		return;

	memcpy(map.pData, indexbuffer->indices.data,
			indexbuffer->num * indexbuffer->indexSize);

	indexbuffer->device->context->Unmap(indexbuffer->indexBuffer, 0);
}

void *indexbuffer_getdata(indexbuffer_t indexbuffer)
{
	return indexbuffer->indices.data;
}

size_t indexbuffer_numindices(indexbuffer_t indexbuffer)
{
	return indexbuffer->num;
}

enum gs_index_type indexbuffer_gettype(indexbuffer_t indexbuffer)
{
	return indexbuffer->type;
}
