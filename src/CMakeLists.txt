# Build injector
add_library(injector SHARED
    injector.cpp
    system.cpp
    bypass.cpp
    inlink.cpp
    inlink_guard.cpp
    resman.cpp
    avatar.cpp
    itemeff.cpp
    resolution.cpp
    mobhptag.cpp
    tooltip.cpp
    itemicon.cpp
    petequipflag.cpp
    proposalgender.cpp
    createblock.cpp
    mobdamage.cpp
    meleerange.cpp
    hpmp.cpp
    buildshow.cpp
    uilayout.cpp
    midair.cpp
    tempstat.cpp
    stringpool.cpp
    debug.cpp
    hook.cpp
)

target_precompile_headers(injector PRIVATE pch.h)
# Build stamp reported at login (cmake/BuildStamp.cmake); slums-build.txt beside slums.dll is for the release tools.
target_compile_definitions(injector PRIVATE SLUMS_BUILD_NUMBER=${SLUMS_BUILD_NUMBER} "SLUMS_RELEASE=\"${SLUMS_RELEASE}\"")
file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/$<CONFIG>/slums-build.txt"
    CONTENT "release=${SLUMS_RELEASE}\nbuild=${SLUMS_BUILD_NUMBER}\n")
# slums.dll's version resource: FileVersion major.minor.patch.build from VERSION and the build (1.6.2.10720),
# ProductVersion the release string, and SlumsBuild the build number, so Explorer and the launcher can read it.
if(SLUMS_BUILD_NUMBER GREATER 65535)
    message(FATAL_ERROR "VERSIONINFO fields are 16-bit; build ${SLUMS_BUILD_NUMBER} does not fit")
endif()
configure_file(slums.rc.in "${CMAKE_CURRENT_BINARY_DIR}/slums.rc" @ONLY)
target_sources(injector PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/slums.rc")
if(MAPLEROOT_MOUNT_INFO)
    target_sources(injector PRIVATE mountinfo.cpp)
    target_compile_definitions(injector PRIVATE MAPLEROOT_MOUNT_INFO)
endif()
if(MAPLEROOT_MOVEMENT_KEYS)
    target_sources(injector PRIVATE movementkeys.cpp)
    target_compile_definitions(injector PRIVATE MAPLEROOT_MOVEMENT_KEYS)
endif()
if(MAPLEROOT_INV_RESIZE)
    target_sources(injector PRIVATE invresize.cpp)
    target_compile_definitions(injector PRIVATE MAPLEROOT_INV_RESIZE)
endif()
if(MAPLEROOT_HUD_METERS)
    if(NOT MAPLEROOT_INTEGRATION)
        message(FATAL_ERROR "MAPLEROOT_HUD_METERS needs MAPLEROOT_INTEGRATION (the HUD and MapleRoot's EXP)")
    endif()
    target_sources(injector PRIVATE hud_meters.cpp)
    target_compile_definitions(injector PRIVATE MAPLEROOT_HUD_METERS)
endif()
if(MAPLEROOT_DISPLAY_TOGGLES)
    if(NOT MAPLEROOT_INTEGRATION)
        message(FATAL_ERROR "MAPLEROOT_DISPLAY_TOGGLES needs MAPLEROOT_INTEGRATION (the HUD window)")
    endif()
    target_sources(injector PRIVATE display.cpp)
    target_compile_definitions(injector PRIVATE MAPLEROOT_DISPLAY_TOGGLES)
endif()
if(MAPLEROOT_DAMAGE_FORMAT)
    if(NOT MAPLEROOT_INTEGRATION)
        message(FATAL_ERROR "MAPLEROOT_DAMAGE_FORMAT needs MAPLEROOT_INTEGRATION (the HUD window switch)")
    endif()
    target_sources(injector PRIVATE damagefmt.cpp)
    target_compile_definitions(injector PRIVATE MAPLEROOT_DAMAGE_FORMAT)
endif()
if(MAPLEROOT_CRASH_REPORT)
    target_sources(injector PRIVATE crash_report.cpp)
    target_compile_definitions(injector PRIVATE MAPLEROOT_CRASH_REPORT)
endif()
if(MAPLEROOT_DAILY_CHECKIN)
    target_sources(injector PRIVATE dailycheckin.cpp)
    target_compile_definitions(injector PRIVATE MAPLEROOT_DAILY_CHECKIN)
endif()
if(MAPLEROOT_SET_EQUIP)
    # tooltip.cpp (always built) gains the set rows and includes tooltip_setitem.inl under the same flag.
    target_sources(injector PRIVATE setitem.cpp setstatbonus.cpp)
    target_compile_definitions(injector PRIVATE MAPLEROOT_SET_EQUIP)
endif()
if(MAPLEROOT_MONSTER_BOOK)
    target_sources(injector PRIVATE monsterbook.cpp monsterbook_foundin.cpp monsterbook_drops.cpp monsterbook_search.cpp)
    target_compile_definitions(injector PRIVATE MAPLEROOT_MONSTER_BOOK)
endif()
if(MAPLEROOT_COLOR_PRISM)
    target_sources(injector PRIVATE weapontint.cpp coloringprism.cpp coloringprism_hooks.cpp)
    target_compile_definitions(injector PRIVATE MAPLEROOT_COLOR_PRISM)
endif()
if(MAPLEROOT_CASHSHOP_WINDOW OR MAPLEROOT_WEATHER OR MAPLEROOT_DAILY_CHECKIN OR MAPLEROOT_SET_EQUIP OR MAPLEROOT_MONSTER_BOOK OR MAPLEROOT_COLOR_PRISM OR MAPLEROOT_INTEGRATION)
    target_sources(injector PRIVATE packetdispatcher.cpp)
endif()
if(MAPLEROOT_WEATHER)
    target_sources(injector PRIVATE weather.cpp weatherfx.cpp weathercanvas.cpp weathersound.cpp
        weathersplash.cpp weatherwind.cpp weatheraccum.cpp weatherpuddle.cpp weathersway.cpp
        weathermove_disabled.cpp lamps.cpp)
    target_compile_definitions(injector PRIVATE MAPLEROOT_WEATHER)
endif()
if(MAPLEROOT_CASHSHOP_WINDOW)
    target_sources(injector PRIVATE cashshopwnd.cpp)
    target_compile_definitions(injector PRIVATE MAPLEROOT_CASHSHOP_WINDOW)
endif()
if(MAPLEROOT_ATTACK_DIAG)
    target_sources(injector PRIVATE attackdiag.cpp)
    target_compile_definitions(injector PRIVATE MAPLEROOT_ATTACK_DIAG)
endif()
if(MAPLEROOT_SHUTDOWN_SMOKE_TEST)
    target_compile_definitions(injector PRIVATE MAPLEROOT_SHUTDOWN_SMOKE_TEST)
endif()
if(MAPLEROOT_INTEGRATION)
    target_sources(injector PRIVATE mapleroot.cpp crash_diagnostics.cpp bounce.cpp)
    # D3D9 host and ImGui overlay; ImGui itself (1.91.4, DX9 + Win32 backends) is built into the companion.
    target_sources(injector PRIVATE d3d9host.cpp settings_overlay.cpp overlay.cpp hud_vitals.cpp hud_native.cpp uitour.cpp)
    target_include_directories(injector PRIVATE "${PROJECT_SOURCE_DIR}/mapleroot/ijl15/ImGui")
    target_compile_definitions(injector PRIVATE MAPLEROOT_INTEGRATION)
    # The MapleRoot companion is part of slums.dll; the game loads the real Intel ijl15.dll again.
    target_link_libraries(injector PRIVATE mapleroot_companion)
    add_custom_command(TARGET injector POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PROJECT_SOURCE_DIR}/vendor/ijl15/ijl15.dll"
            "${PROJECT_SOURCE_DIR}/mapleroot/ijl15/discord_game_sdk.dll"
            "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}"
    )
endif()

target_link_libraries(injector PRIVATE
    Detours
    WzLib
)

set_target_properties(injector PROPERTIES
    OUTPUT_NAME slums
)

add_custom_command(TARGET injector POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/Custom.wz"
        "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}"
)


# Build launcher
add_executable(launcher WIN32
    launcher.manifest
    launcher.rc
    launcher.cpp
    debug.cpp
)

target_link_options(launcher PRIVATE "/MANIFEST:NO")

target_link_libraries(launcher PRIVATE
    Detours
)

set_target_properties(launcher PROPERTIES
    OUTPUT_NAME SlumsMS
)
if(MAPLEROOT_INTEGRATION)
    target_compile_definitions(launcher PRIVATE MAPLEROOT_INTEGRATION)
endif()
