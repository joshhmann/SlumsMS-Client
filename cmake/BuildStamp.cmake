# slums.dll build stamp (CP 0x3738 CLIENT_BUILD_STAMP, sent at login; the server's MIN_CLIENT_BUILD gate reads it).
#
# SLUMS_RELEASE is the VERSION file at the repo root ("1.4.2", "1.4.2-rc1"); SLUMS_BUILD_NUMBER is
# major*10000 + minor*100 + patch of it (1.4.2 -> 10402), so it only grows as releases do. Both are read on every
# configure (VERSION is a configure dependency), so a reused build folder never keeps an old stamp.
# How release builds set it: the release commit bumps VERSION (1.4.2 -> 1.4.3); nothing else. Release candidates keep
# the release's number (1.4.2-rc1 is build 10402), so the gate treats an rc client like the release it leads to.
# A one-off build can pass -DSLUMS_RELEASE_OVERRIDE=... / -DSLUMS_BUILD_NUMBER_OVERRIDE=... (they stay in the
# CMake cache until removed with -U). Both values are written to slums-build.txt next to slums.dll.
file(READ "${PROJECT_SOURCE_DIR}/VERSION" _slums_version)
string(STRIP "${_slums_version}" _slums_version)
if(NOT _slums_version MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)(-[0-9A-Za-z.]+)?$")
    message(FATAL_ERROR "VERSION must look like 1.4.2 or 1.4.2-rc1, not '${_slums_version}'")
endif()
set(_slums_major ${CMAKE_MATCH_1})
set(_slums_minor ${CMAKE_MATCH_2})
set(_slums_patch ${CMAKE_MATCH_3})
if(_slums_minor GREATER 99 OR _slums_patch GREATER 99)
    message(FATAL_ERROR "VERSION minor and patch must be below 100 for the build number")
endif()
math(EXPR _slums_build "${_slums_major} * 10000 + ${_slums_minor} * 100 + ${_slums_patch}")
set(SLUMS_RELEASE "${_slums_version}")
set(SLUMS_BUILD_NUMBER "${_slums_build}")
if(SLUMS_RELEASE_OVERRIDE)
    set(SLUMS_RELEASE "${SLUMS_RELEASE_OVERRIDE}")
endif()
if(SLUMS_BUILD_NUMBER_OVERRIDE)
    set(SLUMS_BUILD_NUMBER "${SLUMS_BUILD_NUMBER_OVERRIDE}")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/VERSION")
if(NOT SLUMS_BUILD_NUMBER MATCHES "^[0-9]+$" OR SLUMS_BUILD_NUMBER GREATER 2147483647)
    message(FATAL_ERROR "SLUMS_BUILD_NUMBER must be a non-negative 32-bit integer")
endif()
if(NOT SLUMS_RELEASE MATCHES "^[0-9A-Za-z._+-]+$")
    message(FATAL_ERROR "SLUMS_RELEASE may only contain letters, digits and . _ + -")
endif()
string(LENGTH "${SLUMS_RELEASE}" _slums_release_length)
if(_slums_release_length GREATER 40)
    message(FATAL_ERROR "SLUMS_RELEASE is longer than 40 characters (the server ignores longer stamps)")
endif()
message(STATUS "slums.dll build stamp: ${SLUMS_RELEASE} (build ${SLUMS_BUILD_NUMBER})")
