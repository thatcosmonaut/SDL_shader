SDL satellite library for shader translation at runtime or build time. Uses glslangValidator and SPIRV-Cross.

Setup:
* `git clone --recursive`
* `cd SDL_shader/external`
* `python ./build_info.py . -i build_info.h.tmpl -o ./glslang/build_info.h`