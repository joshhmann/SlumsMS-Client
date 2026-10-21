add_executable(checked_hook_test checked_hook_test.cpp)
add_test(NAME checked_hook_transactions COMMAND checked_hook_test)

add_executable(quickslot_patch_test quickslot_patch_test.cpp)
add_test(NAME expanded_quickslot_bounds COMMAND quickslot_patch_test)

add_executable(avatar_features_test avatar_features_test.cpp)
add_test(NAME avatar_feature_dependencies COMMAND avatar_features_test)

set(MOVEMENTKEYS_TEST_CLIENT "${CMAKE_SOURCE_DIR}/../Addons/WeatherWork/TestClient" CACHE PATH "Client for loose UI and movement checks")
if(MAPLEROOT_MOVEMENT_KEYS AND (EXISTS "${MOVEMENTKEYS_TEST_CLIENT}/Data/UI/UIWindow.img" OR EXISTS "${MOVEMENTKEYS_TEST_CLIENT}/UI.wz"))
    add_executable(movementkeys_wz_test movementkeys_wz_test.cpp ../src/movementkeys.cpp ../src/hook.cpp ../src/debug.cpp)
    target_link_libraries(movementkeys_wz_test PRIVATE WzLib Detours)
    target_include_directories(movementkeys_wz_test PRIVATE ../src)
    add_test(NAME movementkeys_wz COMMAND movementkeys_wz_test "${MOVEMENTKEYS_TEST_CLIENT}")
endif()

if(MAPLEROOT_WEATHER)
    add_executable(weather_object_policy_test weather_object_policy_test.cpp)
    add_test(NAME weather_animated_portal_colors COMMAND weather_object_policy_test)
endif()

if(MAPLEROOT_MOUNT_INFO)
    add_executable(mountinfo_layout_test mountinfo_layout_test.cpp)
    add_test(NAME mount_preview_layout COMMAND mountinfo_layout_test)
endif()

if(MAPLEROOT_MOUNT_INFO AND EXISTS "${MOVEMENTKEYS_TEST_CLIENT}/Data/Character/TamingMob/01902002.img")
    add_executable(mountinfo_canvas_test mountinfo_canvas_test.cpp)
    target_link_libraries(mountinfo_canvas_test PRIVATE WzLib)
    target_include_directories(mountinfo_canvas_test PRIVATE ../src)
    add_test(NAME mount_full_frame_replacement COMMAND mountinfo_canvas_test "${MOVEMENTKEYS_TEST_CLIENT}")
endif()

if(MAPLEROOT_SET_EQUIP)
    add_executable(setitem_proto_test setitem_proto_test.cpp)
    add_test(NAME setequip_wire_format COMMAND setitem_proto_test)
endif()

if(MAPLEROOT_COLOR_PRISM)
    add_executable(weapontint_proto_test weapontint_proto_test.cpp)
    add_test(NAME colorprism_wire_format COMMAND weapontint_proto_test)
endif()
