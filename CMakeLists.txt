cmake_minimum_required(VERSION 3.16)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

# See docs/release_checklist.md
set(MAJOR_VERSION 3)
set(MINOR_VERSION 0)
set(MICRO_VERSION 0)
set(SDL_REQUIRED_VERSION 3.0.0)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_CURRENT_BINARY_DIR)
    message(FATAL_ERROR "Prevented in-tree built. Please create a build directory outside of the SDL_shader source code and call cmake from there")
endif()

project(SDL3_shader
    LANGUAGES C
    VERSION "${MAJOR_VERSION}.${MINOR_VERSION}.${MICRO_VERSION}"
)

include("${SDL3_shader_SOURCE_DIR}/cmake/GetGitRevisionDescription.cmake" )
include("${SDL3_shader_SOURCE_DIR}/cmake/PrivateSdlFunctions.cmake")
include("${SDL3_shader_SOURCE_DIR}/cmake/sdlmanpages.cmake")
sdl_calculate_derived_version_variables(${MAJOR_VERSION} ${MINOR_VERSION} ${MICRO_VERSION})

message(STATUS "Configuring ${PROJECT_NAME} ${PROJECT_VERSION}")

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(SDL3SHADER_ROOTPROJECT ON)
else()
    set(SDL3SHADER_ROOTPROJECT OFF)
endif()

# Assume MSVC projects don't have a package manager and need vendored dependencies (by default).
# Most other platforms have some kind of package manager.
# FIXME: consider a package manager such as conan/vcpkg instead of vendoring
if(ANDROID OR MSVC)
    set(vendored_default TRUE)
else()
    set(vendored_default FALSE)
endif()

set(sdl3shader_install_enableable ON)
if ((TARGET SDL3-shared OR TARGET SDL3-static OR TARGET SDL3_shader-shared OR TARGET SDL3_shader-static) AND SDL_DISABLE_INSTALL)
    # Cannot install SDL3_shader when SDL3 is built in same built, and is not installed.
    set(sdl3shader_install_enableable OFF)
endif()

include(CMakeDependentOption)
include(CMakePackageConfigHelpers)
include(GNUInstallDirs)
include(CheckSymbolExists)

set(PLATFORM_SUPPORTS_SHARED ON)
if(VITA OR PSP OR PS2 OR N3DS OR RISCOS)
    set(PLATFORM_SUPPORTS_SHARED OFF)
endif()

option(CMAKE_POSITION_INDEPENDENT_CODE "Build static libraries with -fPIC" ${PLATFORM_SUPPORTS_SHARED})
cmake_dependent_option(BUILD_SHARED_LIBS "Build the library as a shared library" ON PLATFORM_SUPPORTS_SHARED OFF)

cmake_dependent_option(SDL3SHADER_INSTALL "Enable SDL3_shader install target" ${SDL3SHADER_ROOTPROJECT} "${sdl3shader_install_enableable}" OFF)
cmake_dependent_option(SDL3SHADER_INSTALL_MAN "Install man pages for SDL3_shader" ${SDL3SHADER_ROOTPROJECT} "SDL3SHADER_INSTALL" OFF)
option(SDL3SHADER_VENDORED "Use vendored third-party libraries" ${vendored_default})
option(SDL3SHADER_WERROR "Treat warnings as errors" OFF)

option(SDL3SHADER_SAMPLES "Build the SDL3_shader sample program(s)" ${SDL3SHADER_ROOTPROJECT})
cmake_dependent_option(SDL3SHADER_SAMPLES_INSTALL "Install the SDL3_shader sample program(s)" OFF "SDL3SHADER_SAMPLES;SDL3SHADER_INSTALL" OFF)

# For style consistency, create a SDL3SHADER_SPIRV CMake variable. This variable is NOT configurable.
set(SDL3SHADER_SPIRV ON)
set(SDL3SHADER_SPIRVCROSS_VENDORED "${SDL3SHADER_VENDORED}")

option(SDL3SHADER_GLSL "Use glslang to translate from GLSL" OFF)
set(SDL3SHADER_GLSLANG_VENDORED "${SDL3SHADER_VENDORED}")

# Save BUILD_SHARED_LIBS variable
set(SDL3SHADER_BUILD_SHARED_LIBS "${BUILD_SHARED_LIBS}")

set(sdl_required_components Headers)

if(SDL3SHADER_BUILD_SHARED_LIBS)
    set(sdl3_shader_target_name SDL3_shader-shared)
    set(sdl3_target_name SDL3::SDL3-shared)

    list(APPEND sdl_required_components SDL3-shared)
else()
    set(sdl3_shader_target_name SDL3_shader-static)
    set(sdl3_target_name SDL3::SDL3)
endif()

if(NOT TARGET SDL3::Headers OR NOT TARGET ${sdl3_target_name})
    find_package(SDL3 ${SDL_REQUIRED_VERSION} REQUIRED COMPONENTS ${sdl_required_components})
endif()

add_library(${sdl3_shader_target_name}
    src/SDL_shader.c
    src/SDL_shader_dummy.c
    src/SDL_shader_d3d11.c
    src/SDL_shader_metal.c
    src/SDL_shader_vulkan.c
)
add_library(SDL3_shader::${sdl3_shader_target_name} ALIAS ${sdl3_shader_target_name})
if(NOT TARGET SDL3_shader::SDL3_shader)
    add_library(SDL3_shader::SDL3_shader ALIAS ${sdl3_shader_target_name})
endif()
target_include_directories(${sdl3_shader_target_name} PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)
target_compile_definitions(${sdl3_shader_target_name} PRIVATE
    BUILD_SDL
    SDL_BUILD_MAJOR_VERSION=${MAJOR_VERSION}
    SDL_BUILD_MINOR_VERSION=${MINOR_VERSION}
    SDL_BUILD_MICRO_VERSION=${MICRO_VERSION}
)
target_link_libraries(${sdl3_shader_target_name} PUBLIC SDL3::Headers)
if(SDL3SHADER_BUILD_SHARED_LIBS)
    target_link_libraries(${sdl3_shader_target_name} PRIVATE SDL3::SDL3-shared)
endif()
sdl_add_warning_options(${sdl3_shader_target_name} WARNING_AS_ERROR ${SDL3SHADER_WERROR})
if(WIN32 AND SDL3SHADER_BUILD_SHARED_LIBS)
    target_sources(${sdl3_shader_target_name} PRIVATE
        src/version.rc
    )
endif()
set_target_properties(${sdl3_shader_target_name} PROPERTIES
    OUTPUT_NAME SDL3_shader
    DEFINE_SYMBOL DLL_EXPORT
    EXPORT_NAME ${sdl3_shader_target_name}
    C_VISIBILITY_PRESET "hidden"
)
if(NOT ANDROID)
    if(APPLE)
        # the SOVERSION property corresponds to the compatibility version and VERSION corresponds to the current version
        # https://cmake.org/cmake/help/latest/prop_tgt/SOVERSION.html#mach-o-versions
        set_target_properties(${sdl3_shader_target_name} PROPERTIES
            SOVERSION "${DYLIB_COMPAT_VERSION}"
            VERSION "${DYLIB_CURRENT_VERSION}"
        )
    else()
        set_target_properties(${sdl3_shader_target_name} PROPERTIES
            SOVERSION "${SO_VERSION_MAJOR}"
            VERSION "${SO_VERSION}"
        )
    endif()
endif()
if(SDL3SHADER_BUILD_SHARED_LIBS)
    if(WIN32)
        set_target_properties(${sdl3_shader_target_name} PROPERTIES
            PREFIX ""
        )
    endif()
else()
    if(MSVC)
        set_target_properties(${sdl3_shader_target_name} PROPERTIES
            OUTPUT_NAME "SDL3_shader-static"
            )
    endif()
endif()

if(SDL3SHADER_BUILD_SHARED_LIBS)
    # Use `Compatible Interface Properties` to ensure a shared SDL3_shader is linked to a shared SDL3 library
    set_property(TARGET ${sdl3_shader_target_name} PROPERTY INTERFACE_SDL3_SHARED ${SDL3SHADER_BUILD_SHARED_LIBS})
    set_property(TARGET ${sdl3_shader_target_name} APPEND PROPERTY COMPATIBLE_INTERFACE_BOOL SDL3_SHARED)
endif()

if(SDL3SHADER_BUILD_SHARED_LIBS)
    sdl_target_link_options_no_undefined(${sdl3_shader_target_name})
endif()

sdl_target_link_option_version_file(${sdl3_shader_target_name} "${CMAKE_CURRENT_SOURCE_DIR}/src/SDL_shader.sym")

if(SDL3SHADER_BUILD_SHARED_LIBS)
    # Make sure static library dependencies are built with -fPIC when building a shared SDL3_shader
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

set(INSTALL_EXTRA_TARGETS)
set(PC_LIBS)
set(PC_REQUIRES)

# Build spirv-cross and glslang as a static library
set(BUILD_SHARED_LIBS OFF)

if(SDL3SHADER_SPIRV)
    if(SDL3SHADER_SPIRVCROSS_VENDORED)
        message(STATUS "${PROJECT_NAME}: Using vendored spirv-cross library")
        # SPIRV_CROSS_ENABLE_CPP variable is used by spirv-cross
        set(SPIRV_CROSS_ENABLE_CPP OFF CACHE BOOL "spirv-cross cpp support" FORCE)
        # SPIRV_CROSS_ENABLE_REFLECT variable is used by spirv-cross
        set(SPIRV_CROSS_ENABLE_REFLECT OFF CACHE BOOL "spirv-cross reflection support" FORCE)
        # SPIRV_CROSS_ENABLE_UTIL variable is used by spirv-cross
        set(SPIRV_CROSS_ENABLE_UTIL OFF CACHE BOOL "spirv-cross util support" FORCE)
        # SPIRV_CROSS_CLI variable is used by spirv-cross
        set(SPIRV_CROSS_CLI OFF CACHE BOOL "spirv-cross CLI support" FORCE)
        # SPIRV_CROSS_ENABLE_TESTS variable is used by spirv-cross
        set(SPIRV_CROSS_ENABLE_TESTS OFF CACHE BOOL "spirv-cross build tests" FORCE)
        if(NOT EXISTS "${PROJECT_SOURCE_DIR}/external/SPIRV-Cross/CMakeLists.txt")
            message(FATAL_ERROR "No spirv-cross sources found. Install a spirv-cross development package or run the download script in the external folder.")
        endif()
        add_subdirectory(external/spirv-cross EXCLUDE_FROM_ALL)
        # spirv-cross is a c++ project, enable c++ here to ensure linking to the c++ standard library
        enable_language(CXX)
        if(NOT SDL3SHADER_BUILD_SHARED_LIBS)
            list(APPEND INSTALL_EXTRA_TARGETS spirv-cross)
            list(APPEND PC_LIBS -l$<TARGET_FILE_BASE_NAME:spirv-cross>)
        endif()
        if(NOT TARGET spirv-cross-c)
            add_library(spirv-cross-c)
        endif()
    else()
        message(STATUS "${PROJECT_NAME}: Using system spirv-cross library")
        find_package(spirv-cross-c REQUIRED)
        list(APPEND PC_REQUIRES spirv-cross-c)
    endif()
    target_compile_definitions(${sdl3_shader_target_name} PRIVATE SDL_SHADER_HAS_SPIRVCROSS=1)
    target_link_libraries(${sdl3_shader_target_name} PRIVATE spirv-cross-c)
endif()

if(SDL3SHADER_GLSL)
    if(SDL3SHADER_GLSLANG_VENDORED)
        message(STATUS "${PROJECT_NAME}: Using vendored glslang library")
        # ENABLE_SPVREMAPPER variable is used by glslang
        set(ENABLE_SPVREMAPPER OFF CACHE BOOL "glslang enable spvremapper" FORCE)
        # ENABLE_GLSLANG_BINARIES variable is used by glslang
        set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "glslang build binaries" FORCE)
        # ENABLE_HLSL variable is used by glslang
        set(ENABLE_HLSL OFF CACHE BOOL "glslang enable hlsl" FORCE)
        # ENABLE_OPT variable is used by glslang
        set(ENABLE_OPT OFF CACHE BOOL "glslang enable spirv-opt" FORCE)
        # ENABLE_PCH variable is used by glslang
        set(ENABLE_PCH OFF CACHE BOOL "glslang enable precompiled headers" FORCE)
        if(NOT EXISTS "${PROJECT_SOURCE_DIR}/external/glslang/CMakeLists.txt")
            message(FATAL_ERROR "No glslang sources found. Install a glslang development package or run the download script in the external folder.")
        endif()
        add_subdirectory(external/glslang EXCLUDE_FROM_ALL)
        # glslang is a c++ project, enable c++ here to ensure linking to the c++ standard library
        enable_language(CXX)
        if(NOT SDL3SHADER_BUILD_SHARED_LIBS)
            list(APPEND INSTALL_EXTRA_TARGETS glslang)
            list(APPEND INSTALL_EXTRA_TARGETS SPIRV)
            list(APPEND INSTALL_EXTRA_TARGETS ResourceLimits)
            list(APPEND PC_LIBS -l$<TARGET_FILE_BASE_NAME:glslang>)
            list(APPEND PC_LIBS -l$<TARGET_FILE_BASE_NAME:SPIRV>)
            list(APPEND PC_LIBS -l$<TARGET_FILE_BASE_NAME:glslang-default-resource-limits>)
        endif()
        if(NOT TARGET glslang)
            add_library(glslang)
        endif()
    else()
        message(STATUS "${PROJECT_NAME}: Using system glslang library")
        find_package(glslang REQUIRED)
        list(APPEND PC_REQUIRES glslang SPIRV glslang-default-resource-limits)
    endif()
    target_compile_definitions(${sdl3_shader_target_name} PRIVATE SDL_SHADER_HAS_GLSLANG=1)
    target_link_libraries(${sdl3_shader_target_name} PRIVATE glslang SPIRV glslang-default-resource-limits)
endif()

# Restore BUILD_SHARED_LIBS variable
set(BUILD_SHARED_LIBS ${SDL3SHADER_BUILD_SHARED_LIBS})

if(SDL3SHADER_INSTALL)
    install(
        TARGETS ${sdl3_shader_target_name}
        EXPORT SDL3_shaderTargets
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT devel
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT library
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT library
    )
    install(
        FILES "${CMAKE_CURRENT_SOURCE_DIR}/include/SDL3_shader/SDL_shader.h"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/SDL3_shader" COMPONENT devel
    )

    if(WIN32 AND NOT MINGW)
        set(SDLSHADER_INSTALL_CMAKEDIR_DEFAULT "cmake")
    else()
        set(SDLSHADER_INSTALL_CMAKEDIR_DEFAULT "${CMAKE_INSTALL_LIBDIR}/cmake/SDL3_shader")
    endif()
    set(SDLSHADER_INSTALL_CMAKEDIR "${SDLSHADER_INSTALL_CMAKEDIR_DEFAULT}" CACHE STRING "Location where to install SDL3_shaderConfig.cmake")
    set(SDLSHADER_PKGCONFIG_INSTALLDIR "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

    configure_package_config_file(cmake/SDL3_shaderConfig.cmake.in SDL3_shaderConfig.cmake
        INSTALL_DESTINATION "${SDLSHADER_INSTALL_CMAKEDIR}"
    )
    write_basic_package_version_file("${PROJECT_BINARY_DIR}/SDL3_shaderConfigVersion.cmake"
        COMPATIBILITY AnyNewerVersion
    )
    install(
        FILES
            "${CMAKE_CURRENT_BINARY_DIR}/SDL3_shaderConfig.cmake"
            "${CMAKE_CURRENT_BINARY_DIR}/SDL3_shaderConfigVersion.cmake"
        DESTINATION ${SDLSHADER_INSTALL_CMAKEDIR}
        COMPONENT devel
    )
    install(EXPORT SDL3_shaderTargets
        FILE "${sdl3_shader_target_name}-targets.cmake"
        NAMESPACE SDL3_shader::
        DESTINATION "${SDLSHADER_INSTALL_CMAKEDIR}"
        COMPONENT devel
    )

    file(RELATIVE_PATH SDL_PATH_PREFIX_RELATIVE_TO_PKGCONFIG "${CMAKE_INSTALL_PREFIX}/${SDLSHADER_PKGCONFIG_INSTALLDIR}" "${CMAKE_INSTALL_PREFIX}")
    string(REGEX REPLACE "[/]+$" "" SDL_PATH_PREFIX_RELATIVE_TO_PKGCONFIG "${SDL_PATH_PREFIX_RELATIVE_TO_PKGCONFIG}")
    set(SDL_PKGCONFIG_PREFIX "\${pcfiledir}/${SDL_PATH_PREFIX_RELATIVE_TO_PKGCONFIG}")

    string(JOIN " " PC_REQUIRES ${PC_REQUIRES})
    string(JOIN " " PC_LIBS ${PC_LIBS})
    configure_file(cmake/sdl3-shader.pc.in sdl3-shader.pc @ONLY)

    # Always install sdl3-shader.pc file: libraries might be different between config modes
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/sdl3-shader.pc"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig" COMPONENT devel)

    install(FILES "LICENSE.txt"
        DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/licenses/${PROJECT_NAME}"
        COMPONENT library
    )

    if(SDL3SHADER_INSTALL_MAN)
        sdl_get_git_revision_hash(SDL3SHADER_REVISION)
        SDL_generate_manpages(
            HEADERS_DIR "${PROJECT_DIR}/include/SDL3_shader"
            SYMBOL "SHD_Init"
            WIKIHEADERS_PL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/build-scripts/wikiheaders.pl"
            REVISION "${SDL3SHADER_REVISION}"
        )
    endif()
endif()

if(SDL3SHADER_SAMPLES)
    # FIXME: Add better samples!
    # add_executable(convertshader examples/convertshader.c)

    # foreach(prog convertshader)
    #     sdl_add_warning_options(${prog} WARNING_AS_ERROR ${SDL3SHADER_WERROR})
    #     target_link_libraries(${prog} PRIVATE SDL3_shader::${sdl3_shader_target_name})
    #     target_link_libraries(${prog} PRIVATE ${sdl3_target_name})

    #     if(SDL3SHADER_SAMPLES_INSTALL)
    #         install(TARGETS ${prog}
    #             RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    #         )
    #     endif()
    # endforeach()
endif()
