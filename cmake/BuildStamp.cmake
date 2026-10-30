# slums.dll build stamp (CP 0x3738 CLIENT_BUILD_STAMP, sent at login; the server's MIN_CLIENT_BUILD gate reads it).
#
# SLUMS_RELEASE is the VERSION file at the repo root ("1.6.2", "1.6.2-rc1"); SLUMS_BUILD_NUMBER is
# major*10000 + (minor+1)*100 + patch*10 of it (1.6.2 -> 10720), so it only grows as releases do. Both are read on
# every configure (VERSION is a configure dependency), so a reused build folder never keeps an old stamp.
# Until 1.6.0 the formula was major*10000 + minor*100 + patch (1.6.0 -> 10600); the 1.6.1 candidates then needed
# overrides (rc11 10700, rc12 10710) and 1.6.2-rc1 was 10720 by override. The new formula gives the same numbers from
# VERSION alone (1.6.1 -> 10710, 1.6.2 -> 10720, 1.6.3 -> 10730, 1.7.0 -> 10800), above every older stamp, with
# room for a hotfix build by override in between (1.6.2 hotfix -> 10721..10729). Ticket b205 (owner, 2026-10-30).
# How release builds set it: the release commit bumps VERSION (1.6.2 -> 1.6.3); nothing else. Release candidates keep
# the release's number (1.6.2-rc1 is build 10720), so the gate treats an rc client like the release it leads to.
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
if(_slums_minor GREATER 98 OR _slums_patch GREATER 9)
    message(FATAL_ERROR "VERSION minor must be below 99 and patch below 10 for the build number")
endif()
math(EXPR _slums_build "${_slums_major} * 10000 + (${_slums_minor} + 1) * 100 + ${_slums_patch} * 10")
# slums.dll's version resource (src/slums.rc.in) shows VERSION's own numbers, not ones read back from the build.
set(SLUMS_VER_MAJOR ${_slums_major})
set(SLUMS_VER_MINOR ${_slums_minor})
set(SLUMS_VER_PATCH ${_slums_patch})
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
