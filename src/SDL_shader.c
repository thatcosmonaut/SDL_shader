#include <SDL3_shader/SDL_shader.h>
#include <build_config/SDL_build_config.h>
#include <spirv_cross_c.h>
#if SDL_SHADER_HAS_GLSLANG
#include <glslang/Include/glslang_c_interface.h>
#include <glslang/Public/resource_limits_c.h>
#endif

/* D3DCompiler API */

static void* d3dcompiler_dll;

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

static void* D3D11_CompileShader(SDL_GpuShaderType shaderType, const char* source, size_t *output_size)
{
	static PFN_D3DCOMPILE D3DCompileFunc;
	const char* profiles[] = {"vs_5_0", "ps_5_0", "cs_5_0"};
	HRESULT result;
	ID3DBlob* blob;
	ID3DBlob* error_blob;
	void* copied_bytecode;

	/* Load the DLL if we haven't already */
	if (!d3dcompiler_dll) {
		d3dcompiler_dll = SDL_LoadObject(D3DCOMPILER_DLL);
		if (!d3dcompiler_dll) {
			SHD_SetError("Failed to load %s", D3DCOMPILER_DLL);
			*output_size = 0;
			return NULL;
		}
	}

	/* Load the D3DCompile function if we haven't already */
	if (!D3DCompileFunc) {
		D3DCompileFunc = (PFN_D3DCOMPILE) SDL_LoadFunction(d3dcompiler_dll, "D3DCompile");
		if (!D3DCompileFunc) {
			SHD_SetError("Failed to load D3DCompile function");
			*output_size = 0;
			return NULL;
		}
	}

	/* Compile! */
	result = D3DCompileFunc(
		source,
		SDL_strlen(source),
		NULL,
		NULL,
		NULL,
		"main",
		profiles[shaderType],
		0,
		0,
		&blob,
		&error_blob
	);
	if (result < 0) {
		SHD_SetError("%s", (const char*) ID3D10Blob_GetBufferPointer(error_blob));
		ID3D10Blob_Release(error_blob);
		*output_size = 0;
		return NULL;
	}

	/* Make a copy of the compiled bytecode */
	*output_size = ID3D10Blob_GetBufferSize(blob);
	copied_bytecode = SDL_malloc(*output_size);
	SDL_memcpy(copied_bytecode, ID3D10Blob_GetBufferPointer(blob), *output_size);
	ID3D10Blob_Release(blob);

	return copied_bytecode;
}
#else
static void* D3D11_CompileShader(SDL_GpuShaderType shaderType, const char* source)
{
	SHD_SetError("SDL was not configured with D3D11 support");
	return NULL;
}
#endif

/* Public API */

const SDL_Version *SHD_Linked_Version(void)
{
    static SDL_Version linked_version;
    SDL_SHADER_VERSION(&linked_version)
    return(&linked_version);
}

int SHD_Init(void)
{
#if SDL_SHADER_HAS_GLSLANG
	if (!glslang_initialize_process()) {
		SHD_SetError("SHD_Init: could not initialize glslang");
		return -1;
	}

	/* Create and preprocess a dummy shader to pre-warm the glslang CommonSymbolTable cache */
	glslang_input_t input;
	SDL_zero(input);
	input.language = GLSLANG_SOURCE_GLSL;
	input.stage = GLSLANG_STAGE_VERTEX;
	input.client = GLSLANG_CLIENT_VULKAN;
	input.client_version = GLSLANG_TARGET_VULKAN_1_0;
	input.target_language = GLSLANG_TARGET_SPV;
	input.target_language_version = GLSLANG_TARGET_SPV_1_0;
	input.code = "void main() { }";
	input.default_version = 450;
	input.default_profile = GLSLANG_NO_PROFILE;
	input.force_default_version_and_profile = SDL_TRUE;
	input.forward_compatible = false;
	input.messages = GLSLANG_MSG_DEFAULT_BIT;
	input.resource = glslang_default_resource();

	glslang_shader_t* shader = glslang_shader_create(&input);
	if (!shader) {
		SHD_SetError("SHD_Init: could not create dummy shader on init");
		return -1;
	}
	if (!glslang_shader_preprocess(shader, &input)) {
		SHD_SetError("SHD_Init: could not preprocess dummy shader on init");
		return -1;
	}
	glslang_shader_delete(shader);
#endif

	return 0;
}

void SHD_Quit(void)
{
#if SDL_SHADER_HAS_GLSLANG
	glslang_finalize_process();
#endif

	if (d3dcompiler_dll) {
		SDL_UnloadObject(d3dcompiler_dll);
	}
}

const void* SHD_TranslateFromGLSL(SDL_GpuBackend backend, SDL_GpuShaderType shaderType, const char* glsl, size_t *output_size)
{
#if SDL_SHADER_HAS_GLSLANG
	glslang_input_t input;
	glslang_stage_t stage;
	glslang_shader_t* shader = NULL;
	glslang_program_t* program = NULL;
	size_t spirv_size = 0;
	void* spirv = NULL;
	const char *spirv_messages = NULL;
	const void* final_output = NULL;

	switch (shaderType)
	{
	case SDL_GPU_SHADERTYPE_VERTEX:
		stage = GLSLANG_STAGE_VERTEX;
		break;
	case SDL_GPU_SHADERTYPE_FRAGMENT:
		stage = GLSLANG_STAGE_FRAGMENT;
		break;
	case SDL_GPU_SHADERTYPE_COMPUTE:
		stage = GLSLANG_STAGE_COMPUTE;
		break;
	default:
		SHD_SetError("SHD_TranslateFromGLSL: unknown shader type");
		return NULL;
	}

	SDL_zero(input);
	input.language = GLSLANG_SOURCE_GLSL;
	input.stage = stage;
	input.client = GLSLANG_CLIENT_VULKAN;
	input.client_version = GLSLANG_TARGET_VULKAN_1_0;
	input.target_language = GLSLANG_TARGET_SPV;
	input.target_language_version = GLSLANG_TARGET_SPV_1_0;
	input.code = glsl;
	input.default_version = 450;
	input.default_profile = GLSLANG_NO_PROFILE;
	input.force_default_version_and_profile = SDL_TRUE;
	input.forward_compatible = false;
	input.messages = GLSLANG_MSG_DEFAULT_BIT;
	input.resource = glslang_default_resource();

	shader = glslang_shader_create(&input);
	if (!shader) {
		SHD_SetError("SHD_TranslateFromGLSL: could not create shader from GLSL");
		return NULL;
	}

	if (!glslang_shader_preprocess(shader, &input)) {
		SHD_SetError("SHD_TranslateFromGLSL: GLSL preprocessing failed: %s | %s", glslang_shader_get_info_log(shader), glslang_shader_get_info_debug_log(shader));
		glslang_shader_delete(shader);
		return NULL;
	}

	if (!glslang_shader_parse(shader, &input)) {
		SHD_SetError("SHD_TranslateFromGLSL: GLSL parsing failed: %s | %s", glslang_shader_get_info_log(shader), glslang_shader_get_info_debug_log(shader));
		glslang_shader_delete(shader);
		return NULL;
	}

	program = glslang_program_create();
	if (!program) {
		SHD_SetError("SHD_TranslateFromGLSL: program creation failed");
		glslang_shader_delete(shader);
		return NULL;
	}

	glslang_program_add_shader(program, shader);

	if (!glslang_program_link(program, GLSLANG_MSG_SPV_RULES_BIT | GLSLANG_MSG_VULKAN_RULES_BIT)) {
		SHD_SetError("SHD_TranslateFromGLSL: program linking failed: %s | %s", glslang_shader_get_info_log(shader), glslang_shader_get_info_debug_log(shader));
		glslang_program_delete(program);
		glslang_shader_delete(shader);
		return NULL;
	}

	glslang_program_SPIRV_generate(program, stage);

	spirv_size = glslang_program_SPIRV_get_size(program) * sizeof(Uint32);
	spirv = SDL_malloc(spirv_size);
	glslang_program_SPIRV_get(program, spirv);

	/* FIXME: What is this...? */
	spirv_messages = glslang_program_SPIRV_get_messages(program);
	if (spirv_messages) {
		SDL_Log("%s", spirv_messages);
	}

	glslang_program_delete(program);
	glslang_shader_delete(shader);

	final_output = SHD_TranslateFromSPIRV(backend, shaderType, spirv, spirv_size, output_size);
	if (backend != SDL_GPU_BACKEND_VULKAN) {
		SDL_free(spirv);
	}
	return final_output;
#else
	SHD_SetError("SDL_shader was compiled without GLSL support");
	return NULL;
#endif
}

const void* SHD_TranslateFromSPIRV(SDL_GpuBackend backend, SDL_GpuShaderType shaderType, const char* spirv, size_t spirv_size, size_t *output_size)
{
	spvc_backend spvc_backend;
	spvc_result result;
	spvc_context context = NULL;
	spvc_parsed_ir ir = NULL;
	spvc_compiler compiler = NULL;
	spvc_compiler_options options = NULL;
	const char* translated = NULL;
	void* final_bytecode = NULL;

	/* Early out for Vulkan since it consumes SPIR-V directly */
	if (backend == SDL_GPU_BACKEND_VULKAN) {
		*output_size = spirv_size;
		return spirv;
	}

	/* Determine which backend to use */
	switch (backend)
	{
	case SDL_GPU_BACKEND_D3D11:
		spvc_backend = SPVC_BACKEND_HLSL;
		break;
	case SDL_GPU_BACKEND_METAL:
		spvc_backend = SPVC_BACKEND_MSL;
		break;
	default:
		SHD_SetError("SHD_TranslateFromSPIRV: unknown GPU backend");
		return NULL;
	}

	/* Create the SPIRV-Cross context */
	result = spvc_context_create(&context);
	if (result < 0) {
		SHD_SetError("SHD_TranslateFromSPIRV: context creation failed: %X", result);
		return NULL;
	}

	/* Parse the SPIR-V into IR */
	result = spvc_context_parse_spirv(context, (const SpvId*) spirv, spirv_size / sizeof(SpvId), &ir);
	if (result < 0) {
		SHD_SetError("SHD_TranslateFromSPIRV: parse failed: %s", spvc_context_get_last_error_string(context));
		spvc_context_destroy(context);
		return NULL;
	}

	/* Set up the cross-compiler */
	result = spvc_context_create_compiler(context, spvc_backend, ir, SPVC_CAPTURE_MODE_TAKE_OWNERSHIP, &compiler);
	if (result < 0) {
		SHD_SetError("SHD_TranslateFromSPIRV: compiler creation failed: %s", spvc_context_get_last_error_string(context));
		spvc_context_destroy(context);
		return NULL;
	}

	result = spvc_compiler_create_compiler_options(compiler, &options);
	if (result < 0) {
		SHD_SetError("SHD_TranslateFromSPIRV: compiler options creation failed: %s", spvc_context_get_last_error_string(context));
		spvc_context_destroy(context);
		return NULL;
	}

	switch (backend)
	{
	case SDL_GPU_BACKEND_D3D11:
		spvc_compiler_options_set_uint(options, SPVC_COMPILER_OPTION_HLSL_SHADER_MODEL, 50);
		break;
	default:
		/* No special settings needed */
		break;
	}

	result = spvc_compiler_install_compiler_options(compiler, options);
	if (result < 0) {
		SHD_SetError("SHD_TranslateFromSPIRV: compiler options installation failed: %s", spvc_context_get_last_error_string(context));
		spvc_context_destroy(context);
		return NULL;
	}

	/* Compile to the target shader language */
	result = spvc_compiler_compile(compiler, &translated);
	if (result < 0) {
		SHD_SetError("SHD_TranslateFromSPIRV: compile failed: %s", spvc_context_get_last_error_string(context));
		spvc_context_destroy(context);
		return NULL;
	}

	/* Compile the translated shader */
	switch (backend)
	{
	case SDL_GPU_BACKEND_D3D11:
		final_bytecode = D3D11_CompileShader(shaderType, translated, output_size);
		break;
	case SDL_GPU_BACKEND_METAL:
		/* FIXME: Compile into MTLLibrary */
		break;
	default:
		/* Just return the shader as-is */
		break;
	}

	/* Clean up */
	spvc_context_destroy(context);

	return final_bytecode;
}
