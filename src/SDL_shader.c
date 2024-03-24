#include <SDL3_shader/SDL_shader.h>
#include <spirv_cross_c.h>
#if SDL_SHADER_HAS_GLSLANG
#include <glslang/Include/glslang_c_interface.h>
#include <glslang/Public/resource_limits_c.h>
#endif

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
#endif

	return 0;
}

void SHD_Quit(void)
{
#if SDL_SHADER_HAS_GLSLANG
	glslang_finalize_process();
#endif
}

const char* SHD_TranslateFromGLSL(SDL_GpuBackend backend, SDL_GpuShaderType shaderType, void* glsl, size_t glsl_size, size_t* output_size)
{
#if SDL_SHADER_HAS_GLSLANG
	glslang_input_t input;
	glslang_stage_t stage;
	glslang_shader_t* shader = NULL;
	glslang_program_t* program = NULL;
	size_t spirv_size = 0;
	void* spirv = NULL;
	const char *spirv_messages = NULL;
	const char *final_output = NULL;

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
	input.default_version = 100;
	input.default_profile = GLSLANG_NO_PROFILE;
	input.force_default_version_and_profile = SDL_FALSE;
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

	final_output = SHD_TranslateFromSPIRV(backend, spirv, spirv_size, output_size);
	SDL_free(spirv);
	return final_output;
#else
	SHD_SetError("SDL_shader was compiled without GLSL support");
	return NULL;
#endif
}

const char* SHD_TranslateFromSPIRV(SDL_GpuBackend backend, void* spirv, size_t spirv_size, size_t *output_size)
{
	spvc_backend spvc_backend;
	spvc_result result;
	spvc_context context = NULL;
	spvc_parsed_ir ir = NULL;
	spvc_compiler compiler = NULL;
	spvc_compiler_options options = NULL;
	const char* translated = NULL;
	char* copied_output = NULL;

	/* Early out for Vulkan since it consumes SPIR-V directly */
	if (backend == SDL_GPU_BACKEND_VULKAN) {
		return (const char*) spirv;
	}

	/* Determine which backend to use */
	switch (backend)
	{
	case SDL_GPU_BACKEND_D3D11:
		spvc_backend = SPVC_BACKEND_HLSL;
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
	result = spvc_context_parse_spirv(context, spirv, spirv_size / 4, &ir);
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
		spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_FLIP_VERTEX_Y, SPVC_TRUE);
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

	/* Make a copy of the translated shader */
	*output_size = SDL_strlen(translated);
	copied_output = SDL_malloc(*output_size);
	SDL_strlcpy(copied_output, translated, *output_size);

	/* Clean up */
	spvc_context_destroy(context);

	return copied_output;
}