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
 * This function creates a shader module for the specified backend by translating GLSL.
 * If needed, it may be first translated into SPIR-V, then to the target language.
 * \param backend The SDL_GPU backend to target
 * \param bytes A pointer to the source GLSL data
 * \param numbytes The length of the source GLSL data, in bytes
 * \returns A shader module compiled with the translated code
 */
extern DECLSPEC SDL_GpuShaderModule* SDLCALL SHD_CreateFromGLSL(SDL_GpuBackend backend, void* bytes, size_t numbytes);

/**
 * This function creates a shader module for the specified backend by translating SPIR-V.
 * \param backend The SDL_GPU backend to target
 * \param bytes A pointer to the source SPIR-V data
 * \param numbytes The length of the source SPIR-V data, in bytes
 * \returns A shader module compiled with the translated code
 */
extern DECLSPEC SDL_GpuShaderModule* SDLCALL SHD_CreateFromSPIRV(SDL_GpuBackend backend, void* bytes, size_t numbytes);

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
