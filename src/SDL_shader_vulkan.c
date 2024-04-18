#include "SDL_shader_internal.h"

#if SDL_GPU_VULKAN

/* Vulkan is just a passthrough */
SHD_Driver VulkanDriver =
{
	SPVC_BACKEND_NONE,
	SDL_TRUE,
	Dummy_Init,
	Dummy_Quit,
	Dummy_SetCompilerOptions,
	Dummy_CompileFromSource
};

#endif