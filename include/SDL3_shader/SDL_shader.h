/*
  SDL_shader:  An example shader translation library for use with SDL
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

/**
 *  \file SDL_shader.h
 *
 *  Header file for SDL_shader library
 *
 * A simple library to translate GLSL and SPIR-V into various shader formats
 */
#ifndef SDL_SHADER_H_
#define SDL_SHADER_H_

#include <SDL3/SDL.h>
#include <SDL3/SDL_begin_code.h>

/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

/**
 * Printable format: "%d.%d.%d", MAJOR, MINOR, PATCHLEVEL
 */
#define SDL_SHADER_MAJOR_VERSION 3
#define SDL_SHADER_MINOR_VERSION 0
#define SDL_SHADER_PATCHLEVEL    0

/**
 * This macro can be used to fill a version structure with the compile-time
 * version of the SDL_shader library.
 */
#define SDL_SHADER_VERSION(X)                        \
{                                                    \
    (X)->major = SDL_SHADER_MAJOR_VERSION;           \
    (X)->minor = SDL_SHADER_MINOR_VERSION;           \
    (X)->patch = SDL_SHADER_PATCHLEVEL;              \
}

/**
 *  This macro will evaluate to true if compiled with SDL_shader at least X.Y.Z.
 */
#define SDL_SHADER_VERSION_ATLEAST(X, Y, Z) \
    ((SDL_SHADER_MAJOR_VERSION >= X) && \
     (SDL_SHADER_MAJOR_VERSION > X || SDL_SHADER_MINOR_VERSION >= Y) && \
     (SDL_SHADER_MAJOR_VERSION > X || SDL_SHADER_MINOR_VERSION > Y || SDL_SHADER_PATCHLEVEL >= Z))

/**
 * This function gets the version of the dynamically linked SDL_shader library.
 *
 * it should NOT be used to fill a version structure, instead you should use
 * the SDL_SHADER_VERSION() macro.
 *
 * \returns SDL_shader version
 */
extern DECLSPEC const SDL_Version * SDLCALL SHD_Linked_Version(void);

/**
 * This function initializes the SDL_shader system.
 * It MUST be called before calling any other SHD_Translate* APIs!
 * \returns Success code (0 on success, -1 on failure)
 */
extern DECLSPEC int SDLCALL SHD_Init(void);

/**
 * This function closes the SDL_shader system.
 * Do NOT call SHD_* APIs after this has been called!
 */
extern DECLSPEC void SDLCALL SHD_Quit(void);

/**
 * This function translates source GLSL into shader code for the specified backend.
 * If needed, the GLSL source may be first translated into SPIR-V, then to the target language.
 * The client is responsible for freeing the memory returned by this call.
 * \param backend The SDL_GPU backend to target
 * \param shaderType The SDL_GpuShaderType of the GLSL
 * \param glsl The GLSL source code
 * \param output_size Pointer to a size_t to hold the length of the output shader data, in bytes
 * \returns The translated shader bytes to pass into SDL_GpuShaderModuleCreateInfo, or NULL on failure
 */
extern DECLSPEC const char* SDLCALL SHD_TranslateFromGLSL(SDL_GpuBackend backend, SDL_GpuShaderType shaderType, const char* glsl, size_t* output_size);

/**
 * This function translates source SPIR-V into shader code for the specified backend.
 * The client is responsible for freeing the memory returned by this call.
 * \param backend The SDL_GPU backend to target
 * \param spirv A pointer to the source SPIR-V data
 * \param spirv_size The length of the source SPIR-V data, in bytes
 * \param output_size Pointer to a size_t to hold the length of the output shader data, in bytes
 * \returns The translated shader bytes to pass into SDL_GpuShaderModuleCreateInfo, or NULL on failure
 */
extern DECLSPEC const char* SDLCALL SHD_TranslateFromSPIRV(SDL_GpuBackend backend, const char* spirv, size_t spirv_size, size_t* output_size);

/**
 * Report SDL_shader errors
 *
 * \sa SHD_GetError
 */
#define SHD_SetError    SDL_SetError

/**
 * Get last SDL_shader error
 *
 * \sa SHD_SetError
 */
#define SHD_GetError    SDL_GetError

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif
#include <SDL3/SDL_close_code.h>

#endif /* SDL_SHADER_H_ */
