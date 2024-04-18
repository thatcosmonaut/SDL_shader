#include "SDL_shader_internal.h"

int Dummy_Init(void)
{
	return 0;
}

void Dummy_Quit(void) { }

void Dummy_SetCompilerOptions(spvc_compiler_options options)
{
	(void)options;
}

SDL_GpuShaderModule* Dummy_CompileFromSource(SDL_GpuDevice* device, SDL_GpuShaderStage shader_stage, const char* source)
{
	(void)shader_stage;
	(void)source;
	return NULL;
}