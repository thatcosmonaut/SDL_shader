#include "SDL_shader_internal.h"

#if SDL_GPU_D3D11

#define CINTERFACE
#define COBJMACROS
#include <d3dcompiler.h>

/* __stdcall declaration, largely taken from vkd3d_windows.h */
#ifdef _WIN32
#define D3DCOMPILER_API __stdcall
#else
# ifdef __stdcall
#  undef __stdcall
# endif
# ifdef __x86_64__
#  define __stdcall __attribute__((ms_abi))
# else
#  if (__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ >= 2)) || defined(__APPLE__)
#   define __stdcall __attribute__((__stdcall__)) __attribute__((__force_align_arg_pointer__))
#  else
#   define __stdcall __attribute__((__stdcall__))
#  endif
# endif
# define D3DCOMPILER_API __stdcall
#endif

/* vkd3d uses stdcall for its ID3D10Blob implementation */
#ifndef _WIN32
typedef struct VKD3DBlob VKD3DBlob;
typedef struct VKD3DBlobVtbl
{
	HRESULT(__stdcall* QueryInterface)(
		VKD3DBlob* This,
		REFIID riid,
		void** ppvObject);
	ULONG(__stdcall* AddRef)(VKD3DBlob* This);
	ULONG(__stdcall* Release)(VKD3DBlob* This);
	LPVOID(__stdcall* GetBufferPointer)(VKD3DBlob* This);
	SIZE_T(__stdcall* GetBufferSize)(VKD3DBlob* This);
} VKD3DBlobVtbl;
struct VKD3DBlob
{
	const VKD3DBlobVtbl* lpVtbl;
};
#define ID3D10Blob VKD3DBlob
#define ID3DBlob VKD3DBlob
#endif

/* rename the DLL for different platforms */
#if defined(WIN32)
#undef D3DCOMPILER_DLL
#define D3DCOMPILER_DLL D3DCOMPILER_DLL_A
#elif defined(__APPLE__)
#undef D3DCOMPILER_DLL
#define D3DCOMPILER_DLL "libvkd3d-utils.1.dylib"
#else
#undef D3DCOMPILER_DLL
#define D3DCOMPILER_DLL "libvkd3d-utils.so.1"
#endif

/* D3DCompile signature */
typedef HRESULT(D3DCOMPILER_API* PFN_D3DCOMPILE)(
	LPCVOID pSrcData,
	SIZE_T SrcDataSize,
	LPCSTR pSourceName,
	const D3D_SHADER_MACRO* pDefines,
	ID3DInclude* pInclude,
	LPCSTR pEntrypoint,
	LPCSTR pTarget,
	UINT Flags1,
	UINT Flags2,
	ID3DBlob** ppCode,
	ID3DBlob** ppErrorMsgs
);

const char* profiles[] = { "vs_5_0", "ps_5_0", "cs_5_0" };

static void* d3dcompiler_dll;
static PFN_D3DCOMPILE D3DCompile_func;

static int D3D11_Init(void)
{
	/* Load the DLL if we haven't already */
	if (!d3dcompiler_dll) {
		d3dcompiler_dll = SDL_LoadObject(D3DCOMPILER_DLL);
		if (!d3dcompiler_dll) {
			SHD_SetError("Failed to load " D3DCOMPILER_DLL);
			return -1;
		}
	}

	/* Load the D3DCompile function if we haven't already */
	if (!D3DCompile_func) {
		D3DCompile_func = (PFN_D3DCOMPILE) SDL_LoadFunction(d3dcompiler_dll, "D3DCompile");
		if (!D3DCompile_func) {
			SHD_SetError("Failed to load D3DCompile function");
			return -1;
		}
	}

	return 0;
}

static void D3D11_Quit(void)
{
	if (d3dcompiler_dll) {
		SDL_UnloadObject(d3dcompiler_dll);
	}
	d3dcompiler_dll = NULL;
	D3DCompile_func = NULL;
}

static void D3D11_SetCompilerOptions(spvc_compiler_options options)
{
	spvc_compiler_options_set_uint(options, SPVC_COMPILER_OPTION_HLSL_SHADER_MODEL, 50);
}

static SDL_GpuShaderModule* D3D11_CompileFromSource(SDL_GpuDevice* device, SDL_GpuShaderStage shader_stage, const char* source)
{
	HRESULT result;
	ID3DBlob *blob;
	ID3DBlob *error_blob;
	SDL_GpuShaderModuleCreateInfo createinfo;
	SDL_GpuShaderModule *shader_module;

	/* Compile! */
	result = D3DCompile_func(
		source,
		SDL_strlen(source),
		NULL,
		NULL,
		NULL,
		"main",
		profiles[shader_stage],
		0,
		0,
		&blob,
		&error_blob
	);
	if (result < 0) {
		SHD_SetError("%s", (const char*) ID3D10Blob_GetBufferPointer(error_blob));
		ID3D10Blob_Release(error_blob);
		return NULL;
	}

	/* Create the shader module */
	createinfo.code = ID3D10Blob_GetBufferPointer(blob);
	createinfo.codeSize = ID3D10Blob_GetBufferSize(blob);
	createinfo.format = SDL_GPU_SHADERFORMAT_DXBC;
	createinfo.stage = shader_stage;
	shader_module = SDL_GpuCreateShaderModule(device, &createinfo);

	/* Clean up */
	ID3D10Blob_Release(blob);

	return shader_module;
}

SHD_Driver D3D11Driver =
{
	SPVC_BACKEND_HLSL,
	SDL_FALSE,
	D3D11_Init,
	D3D11_Quit,
	D3D11_SetCompilerOptions,
	D3D11_CompileFromSource
};

#endif