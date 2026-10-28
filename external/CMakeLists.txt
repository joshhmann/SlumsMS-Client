# Build Detours
add_library(Detours STATIC
        Detours/src/creatwth.cpp
        Detours/src/modules.cpp
        Detours/src/detours.cpp
        Detours/src/disolx86.cpp
        Detours/src/disasm.cpp
        Detours/src/image.cpp
)
target_include_directories(Detours PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Detours/src)


# Build WzLib
add_subdirectory(WzLib)


# Build FreeType (2.13.3, submodule) for crisp ImGui text: no zlib/bzip2/png/harfbuzz/brotli, static.
set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
set(SKIP_INSTALL_ALL ON CACHE BOOL "" FORCE)
add_subdirectory(freetype EXCLUDE_FROM_ALL)


# Build RmlUi (6.3, submodule) for the HUD skin: static core, FreeType font engine on the in-tree FreeType above,
# no samples, no Lua, no SVG/Lottie. RmlUi treats FreeType as a soft dependency, so the alias is all it needs.
if(MAPLEROOT_RMLUI)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/RmlUi/CMakeLists.txt")
        message(FATAL_ERROR "MAPLEROOT_RMLUI needs the external/RmlUi submodule (git submodule update --init external/RmlUi)")
    endif()
    add_library(Freetype::Freetype ALIAS freetype)
    set(BUILD_SHARED_LIBS OFF)
    set(RMLUI_FONT_ENGINE "freetype" CACHE STRING "" FORCE)
    set(RMLUI_SAMPLES OFF CACHE BOOL "" FORCE)
    set(RMLUI_LUA_BINDINGS OFF CACHE BOOL "" FORCE)
    set(RMLUI_SVG_PLUGIN OFF CACHE BOOL "" FORCE)
    set(RMLUI_LOTTIE_PLUGIN OFF CACHE BOOL "" FORCE)
    set(RMLUI_COMPILER_OPTIONS OFF CACHE BOOL "" FORCE)
    set(RMLUI_INSTALL_LICENSES_AND_BUILD_INFO OFF CACHE BOOL "" FORCE)
    add_subdirectory(RmlUi EXCLUDE_FROM_ALL)
endif()
