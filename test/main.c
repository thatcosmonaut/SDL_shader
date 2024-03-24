#include <SDL3_shader/SDL_shader.h>

int main(int argc, char** argv)
{
    const SDL_Version *sdlShaderVersion = SHD_Linked_Version();
    SDL_Log("Linked with SDL_shader v%d.%d.%d", sdlShaderVersion->major, sdlShaderVersion->minor, sdlShaderVersion->patch);

    /* Initialize the library */
    if (SHD_Init() != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s\n", SDL_GetError());
        return 1;
    }

    /* Direct SPIR-V conversion */
    size_t spirvLength;
    void* spirvInput = SDL_LoadFile("SolidColor.frag.spv", &spirvLength);
    if (!spirvInput)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s\n", SDL_GetError());
        return 1;
    }

    size_t hlslLength;
    const char *hlslText = SHD_TranslateFromSPIRV(SDL_GPU_BACKEND_D3D11, spirvInput, spirvLength, &hlslLength);
    if (!hlslText)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s\n", SHD_GetError());
        return 1;
    }

    SDL_Log("SPIRV: Produced shader of size %llu:\n%s\n\n", hlslLength, hlslText);
    SDL_free((void*) hlslText);
    SDL_free(spirvInput);

    /* GLSL -> SPIR-V -> HLSL conversion */
    size_t glslLength;
    void* glslInput = SDL_LoadFile("SolidColor.frag", &glslLength);
    if (!glslInput)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s\n", SDL_GetError());
        return 1;
    }

    hlslText = SHD_TranslateFromGLSL(SDL_GPU_BACKEND_D3D11, SDL_GPU_SHADERTYPE_FRAGMENT, glslInput, glslLength, &hlslLength);
    if (!hlslText)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s\n", SHD_GetError());
        return 1;
    }

    SDL_Log("GLSL: Produced shader of size %llu:\n%s\n\n", hlslLength, hlslText);

    SDL_free((void*) hlslText);
    SDL_free(glslInput);

    SHD_Quit();

    return 0;
}