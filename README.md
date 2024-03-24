SDL satellite library for shader translation at runtime or build time. Uses SPIRV-Cross and (optionally) glslangValidator.

Setup:
* `git clone --recursive`
* `mkdir build`
* `cmake .. -DSDL3SHADER_GLSL=ON` (omit if GLSL source translation is not desired)