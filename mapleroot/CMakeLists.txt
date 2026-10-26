# The MapleRoot companion, linked into slums.dll as a static library (it used to ship as a separate ijl15.dll
# proxy that Kaentake loaded through a bridge). The ijl15 proxy itself (Proxy/) is not built: the game loads the
# real Intel ijl15.dll again. Compiled the way ijl15.vcxproj compiled it.
add_library(mapleroot_companion STATIC
    ijl15/API/MapleStory/Type/SKILLENTRY.cpp
    ijl15/API/MapleStory/Type/ZXString.cpp
    ijl15/Discord/achievement_manager.cpp
    ijl15/Discord/activity_manager.cpp
    ijl15/Discord/application_manager.cpp
    ijl15/Discord/core.cpp
    ijl15/Discord/image_manager.cpp
    ijl15/Discord/lobby_manager.cpp
    ijl15/Discord/network_manager.cpp
    ijl15/Discord/overlay_manager.cpp
    ijl15/Discord/relationship_manager.cpp
    ijl15/Discord/storage_manager.cpp
    ijl15/Discord/store_manager.cpp
    ijl15/Discord/types.cpp
    ijl15/Discord/user_manager.cpp
    ijl15/Discord/voice_manager.cpp
    ijl15/discord_worker.cpp
    ijl15/DllMain.cpp
    ijl15/Funcs.cpp
    ijl15/ImGui/imgui.cpp
    ijl15/ImGui/imgui_demo.cpp
    ijl15/ImGui/imgui_draw.cpp
    ijl15/ImGui/imgui_impl_dx9.cpp
    ijl15/ImGui/imgui_impl_win32.cpp
    ijl15/ImGui/imgui_tables.cpp
    ijl15/ImGui/imgui_widgets.cpp
    ijl15/ImGui/misc/freetype/imgui_freetype.cpp
    ijl15/MapleClientCollectionTypes/ZAllocEx.cpp
    ijl15/MapleHook.cpp
    ijl15/NMCO/NMDataObject.cpp
    ijl15/NMCO/NMFunctionObject.cpp
    ijl15/NMCO/NMSerializable.cpp
    ijl15/Overlay/Drawing.cpp
    ijl15/Overlay/Hook.cpp
    ijl15/theactualstuff/BossHP.cpp
    ijl15/theactualstuff/res.cpp
    ijl15/SkillEdits/Activeskill.cpp
    ijl15/theactualstuff/user.cpp
    ijl15/Util/Gfx.cpp
    ijl15/Util/MemPatch.cpp
    ijl15/Util/Util.cpp
    ijl15/WinHook.cpp
)
target_compile_definitions(mapleroot_companion PRIVATE
    MAPLEROOT_KAENTAKE MAPLEROOT_SINGLE_DLL _MBCS _WINSOCK_DEPRECATED_NO_WARNINGS _CRT_SECURE_NO_WARNINGS)
target_compile_options(mapleroot_companion PRIVATE /std:c++latest /permissive- /sdl /W0 /EHsc)
target_include_directories(mapleroot_companion PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/ijl15/include/wil
    ${CMAKE_CURRENT_SOURCE_DIR}/ijl15/ImGui)

# Detours 4 (Global.h includes <detours.h> in single-DLL mode) and the Discord SDK import library.
target_link_libraries(mapleroot_companion PUBLIC
    freetype
    Detours
    ${CMAKE_CURRENT_SOURCE_DIR}/ijl15/discord_game_sdk.dll.lib
    comsuppw winmm ws2_32 d3d9)
