# Build injector
add_library(injector SHARED
    injector.cpp
    system.cpp
    bypass.cpp
    inlink.cpp
    resman.cpp
    avatar.cpp
    itemeff.cpp
    resolution.cpp
    mobhptag.cpp
    tooltip.cpp
    itemicon.cpp
    tempstat.cpp
    stringpool.cpp
    debug.cpp
    hook.cpp
)

target_precompile_headers(injector PRIVATE pch.h)
if(MAPLEROOT_MOUNT_INFO)
    target_sources(injector PRIVATE mountinfo.cpp)
    target_compile_definitions(injector PRIVATE MAPLEROOT_MOUNT_INFO)
endif()
if(MAPLEROOT_MOVEMENT_KEYS)
    target_sources(injector PRIVATE movementkeys.cpp)
    target_compile_definitions(injector PRIVATE MAPLEROOT_MOVEMENT_KEYS)
endif()
if(MAPLEROOT_CASHSHOP_WINDOW OR MAPLEROOT_WEATHER)
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
if(MAPLEROOT_SHUTDOWN_SMOKE_TEST)
    target_compile_definitions(injector PRIVATE MAPLEROOT_SHUTDOWN_SMOKE_TEST)
endif()
if(MAPLEROOT_INTEGRATION)
    target_sources(injector PRIVATE mapleroot.cpp crash_diagnostics.cpp)
    target_compile_definitions(injector PRIVATE MAPLEROOT_INTEGRATION)
endif()

target_link_libraries(injector PRIVATE
    Detours
    WzLib
)

set_target_properties(injector PROPERTIES
    OUTPUT_NAME ${PROJECT_NAME}
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
    OUTPUT_NAME ${PROJECT_NAME}
)
if(MAPLEROOT_INTEGRATION)
    target_compile_definitions(launcher PRIVATE MAPLEROOT_INTEGRATION)
endif()
