#include "SDL_shader_internal.h"

#if SDL_GPU_METAL

static SDL_GpuShader* Metal_CompileFromSource(SDL_GpuDevice *device, SDL_GpuShaderStage shader_stage, const char* entryPointName, const char* source)
{
	SDL_GpuShaderCreateInfo createinfo;
	createinfo.code = (const Uint8*) source;
	createinfo.codeSize = SDL_strlen(source);
	createinfo.format = SDL_GPU_SHADERFORMAT_MSL;
	createinfo.stage = shader_stage;
	createinfo.entryPointName = entryPointName;
	return SDL_GpuCreateShader(device, &createinfo);
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
