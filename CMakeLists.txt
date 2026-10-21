cmake_minimum_required(VERSION 3.14)
project(SlumsMS)
option(MAPLEROOT_MOUNT_INFO "Fit character-info mount previews and names" OFF)
option(MAPLEROOT_MOVEMENT_KEYS "Build bindable direction keys in keyboard settings" OFF)
option(MAPLEROOT_WEATHER "Build visual weather/time and lamps with matching server support" OFF)
option(MAPLEROOT_INTEGRATION "Preserve MapleRoot custom gameplay and Data assets" OFF)
option(MAPLEROOT_SHUTDOWN_SMOKE_TEST "Test-only client: return from Run immediately to exercise native cleanup" OFF)
option(MAPLEROOT_DAILY_CHECKIN "Build the Daily Check-In window (requires matching server support)" OFF)
option(MAPLEROOT_SET_EQUIP "Build the SetEquip set tooltip and Ability-window bonus (requires matching server support)" OFF)
option(MAPLEROOT_CASHSHOP_WINDOW "Build the in-field cash shop addon (requires matching server support)" OFF)
if(MAPLEROOT_SHUTDOWN_SMOKE_TEST AND NOT MAPLEROOT_INTEGRATION)
    message(FATAL_ERROR "Shutdown smoke tests require MAPLEROOT_INTEGRATION")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")

# Build external dependencies
add_subdirectory(external)

if(MAPLEROOT_INTEGRATION)
    add_subdirectory(mapleroot)
endif()

# Build injector and laucher
add_subdirectory(src)
include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
