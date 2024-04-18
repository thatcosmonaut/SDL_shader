#include "SDL_shader_internal.h"

#if SDL_GPU_METAL

static SDL_GpuShaderModule* Metal_CompileFromSource(SDL_GpuDevice *device, SDL_GpuShaderStage shader_stage, const char* source)
{
	SDL_GpuShaderModuleCreateInfo createinfo;
	SDL_GpuShaderModule* shader_module;

	createinfo.code = source;
	createinfo.codeSize = SDL_strlen(source);
	createinfo.format = SDL_GPU_SHADERFORMAT_MSL;
	createinfo.stage = shader_stage;

	return SDL_GpuCreateShaderModule(device, &shader_module);
}

SHD_Driver MetalDriver =
{
	SPVC_BACKEND_MSL,
	SDL_FALSE,
	Dummy_Init,
	Dummy_Quit,
	Dummy_SetCompilerOptions,
	Metal_CompileFromSource
};

#endif