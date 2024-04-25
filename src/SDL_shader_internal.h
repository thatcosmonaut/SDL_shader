#ifndef SDL_SHADER_INTERNAL_H
#define SDL_SHADER_INTERNAL_H

#include <SDL3_shader/SDL_shader.h>
#include <build_config/SDL_build_config.h>
#include <spirv_cross_c.h>

typedef struct SHD_Driver
{
	spvc_backend SpvcBackend;
	SDL_bool ConsumesSPIRV;
	int (*Init)(void);
	void (*Quit)(void);
	void (*SetCompilerOptions)(spvc_compiler_options options);
	SDL_GpuShader* (*CompileFromSource)(SDL_GpuDevice* device, SDL_GpuShaderStageFlagBits shader_stage, const char* entryPointName, const char* source);
} SHD_Driver;

/* FIXME: Do we need some sort of visibility attribute here? */
int Dummy_Init(void);
void Dummy_Quit(void);
void Dummy_SetCompilerOptions(spvc_compiler_options options);
SDL_GpuShader* Dummy_CompileFromSource(SDL_GpuDevice* device, SDL_GpuShaderStageFlagBits shader_stage, const char* entryPointName, const char* source);

extern SHD_Driver VulkanDriver;
extern SHD_Driver D3D11Driver;
extern SHD_Driver MetalDriver;

#endif
