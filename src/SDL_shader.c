#include "SDL_shader_internal.h"
#if SDL_SHADER_HAS_GLSLANG
#include <glslang/Include/glslang_c_interface.h>
#include <glslang/Public/resource_limits_c.h>
#endif

static int initialized;
static SHD_Driver *shd_driver;
static SDL_GpuDevice *gpu_device;

const SDL_Version *SHD_Linked_Version(void)
{
    static SDL_Version linked_version;
    SDL_SHADER_VERSION(&linked_version)
    return(&linked_version);
}

int SHD_Init(SDL_GpuDevice *device)
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

	if (!device) {
		SHD_SetError("SHD_Init: device is null");
		return -1;
	}

	switch (SDL_GpuGetBackend(device))
	{
#if SDL_GPU_VULKAN
	case SDL_GPU_BACKEND_VULKAN:
		shd_driver = &VulkanDriver;
		break;
#endif
#if SDL_GPU_D3D11
	case SDL_GPU_BACKEND_D3D11:
		shd_driver = &D3D11Driver;
		break;
#endif
#if SDL_GPU_METAL
	case SDL_GPU_BACKEND_METAL:
		shd_driver = &MetalDriver;
		break;
#endif
	default:
		SHD_SetError("SHD_Init: Unsupported backend");
		return -1;
	}

	if (shd_driver->Init() < 0) {
		return -1;
	}
	gpu_device = device;
	initialized = 1;
	return 0;
}

void SHD_Quit(void)
{
	if (initialized) {
		shd_driver->Quit();
		shd_driver = NULL;
		gpu_device = NULL;
		initialized = 0;
#if SDL_SHADER_HAS_GLSLANG
		glslang_finalize_process();
#endif
	}
}

SDL_GpuShaderModule* SHD_CreateShaderModuleFromGLSL(SDL_GpuShaderStage shader_stage, const char* glsl)
{
#if SDL_SHADER_HAS_GLSLANG
	glslang_input_t input;
	glslang_stage_t stage;
	glslang_shader_t* shader = NULL;
	glslang_program_t* program = NULL;
	size_t spirv_size = 0;
	void* spirv = NULL;
	const char *spirv_messages = NULL;
	SDL_GpuShaderModule *shader_module = NULL;

	switch (shader_stage)
	{
	case SDL_GPU_SHADERSTAGE_VERTEX:
		stage = GLSLANG_STAGE_VERTEX;
		break;
	case SDL_GPU_SHADERSTAGE_FRAGMENT:
		stage = GLSLANG_STAGE_FRAGMENT;
		break;
	case SDL_GPU_SHADERSTAGE_COMPUTE:
		stage = GLSLANG_STAGE_COMPUTE;
		break;
	default:
		SHD_SetError("SHD_TranslateFromGLSL: unknown shader stage");
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

	shader_module = SHD_CreateShaderModuleFromSPIRV(shader_stage, spirv, spirv_size);
	SDL_free(spirv);
	return shader_module;
#else
	SHD_SetError("SDL_shader was compiled without GLSL support");
	return NULL;
#endif
}

SDL_GpuShaderModule* SHD_CreateShaderModuleFromSPIRV(SDL_GpuShaderStage shader_stage, const char* spirv, size_t spirv_size)
{
	SDL_GpuShaderModuleCreateInfo createinfo;
	SDL_GpuShaderModule *shader_module;
	spvc_result result;
	spvc_context context = NULL;
	spvc_parsed_ir ir = NULL;
	spvc_compiler compiler = NULL;
	spvc_compiler_options options = NULL;
	const char* translated = NULL;

	/* Fast path for drivers that consume SPIR-V directly */
	if (shd_driver->ConsumesSPIRV) {
		createinfo.code = spirv;
		createinfo.codeSize = spirv_size;
		createinfo.stage = shader_stage;
		createinfo.format = SDL_GPU_SHADERFORMAT_SPIRV;
		return SDL_GpuCreateShaderModule(gpu_device, &createinfo);
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

	/* Create the cross-compiler */
	result = spvc_context_create_compiler(context, shd_driver->SpvcBackend, ir, SPVC_CAPTURE_MODE_TAKE_OWNERSHIP, &compiler);
	if (result < 0) {
		SHD_SetError("SHD_TranslateFromSPIRV: compiler creation failed: %s", spvc_context_get_last_error_string(context));
		spvc_context_destroy(context);
		return NULL;
	}

	/* Set up the cross-compiler options */
	result = spvc_compiler_create_compiler_options(compiler, &options);
	if (result < 0) {
		SHD_SetError("SHD_TranslateFromSPIRV: compiler options creation failed: %s", spvc_context_get_last_error_string(context));
		spvc_context_destroy(context);
		return NULL;
	}

	shd_driver->SetCompilerOptions(options);

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

	/* Compile the shader module */
	shader_module = shd_driver->CompileFromSource(gpu_device, shader_stage, translated);

	/* Clean up */
	spvc_context_destroy(context);

	return shader_module;
}
