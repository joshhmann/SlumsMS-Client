add_executable(buildstamp_proto_test buildstamp_proto_test.cpp)
target_include_directories(buildstamp_proto_test PRIVATE ../src)
target_compile_definitions(buildstamp_proto_test PRIVATE SLUMS_BUILD_NUMBER=${SLUMS_BUILD_NUMBER} "SLUMS_RELEASE=\"${SLUMS_RELEASE}\"")
add_test(NAME buildstamp_wire_format COMMAND buildstamp_proto_test)

add_executable(buildshow_test buildshow_test.cpp)
add_test(NAME buildshow_text COMMAND buildshow_test)

add_executable(checked_hook_test checked_hook_test.cpp)
add_test(NAME checked_hook_transactions COMMAND checked_hook_test)

add_executable(quickslot_patch_test quickslot_patch_test.cpp)
add_test(NAME expanded_quickslot_bounds COMMAND quickslot_patch_test)

add_executable(avatar_features_test avatar_features_test.cpp)
add_test(NAME avatar_feature_dependencies COMMAND avatar_features_test)

add_executable(petequip_flag_test petequip_flag_test.cpp)
add_test(NAME petequip_flag_bounds COMMAND petequip_flag_test)

add_executable(proposal_patch_test proposal_patch_test.cpp)
add_test(NAME proposal_any_gender COMMAND proposal_patch_test)

add_executable(createblock_patch_test createblock_patch_test.cpp)
set(CREATEBLOCK_TEST_EXE "" CACHE FILEPATH "MapleStory.exe whose bytes the create-block test checks (optional)")
if(CREATEBLOCK_TEST_EXE AND EXISTS "${CREATEBLOCK_TEST_EXE}")
    add_test(NAME createblock_never_blocked COMMAND createblock_patch_test "${CREATEBLOCK_TEST_EXE}")
else()
    add_test(NAME createblock_never_blocked COMMAND createblock_patch_test)
endif()

add_executable(invresize_patch_test invresize_patch_test.cpp)
set(INVRESIZE_TEST_EXE "Z:/MSv83/SlumsMS-dev/client-1.5.0-fixes2/exe-copy/MapleStory.exe" CACHE FILEPATH
    "MapleStory.exe whose bytes the inventory-resize test checks (optional)")
if(INVRESIZE_TEST_EXE AND EXISTS "${INVRESIZE_TEST_EXE}")
    add_test(NAME invresize_sites COMMAND invresize_patch_test "${INVRESIZE_TEST_EXE}")
else()
    add_test(NAME invresize_sites COMMAND invresize_patch_test)
endif()

add_executable(mobdamage_patch_test mobdamage_patch_test.cpp)
set(MOBDAMAGE_TEST_EXE "Z:/MSv83/SlumsMS-dev/client-1.5.0-fixes2/exe-copy/MapleStory.exe" CACHE FILEPATH
    "MapleStory.exe whose bytes the mob-damage test checks (optional)")
if(MOBDAMAGE_TEST_EXE AND EXISTS "${MOBDAMAGE_TEST_EXE}")
    add_test(NAME mobdamage_low_defence COMMAND mobdamage_patch_test "${MOBDAMAGE_TEST_EXE}")
else()
    add_test(NAME mobdamage_low_defence COMMAND mobdamage_patch_test)
endif()

add_executable(meleerange_patch_test meleerange_patch_test.cpp)
set(MELEERANGE_TEST_EXE "Z:/MSv83/SlumsMS-dev/client-1.5.0-fixes2/exe-copy/MapleStory.exe" CACHE FILEPATH
    "MapleStory.exe whose bytes the remote melee range test checks (optional)")
if(MELEERANGE_TEST_EXE AND EXISTS "${MELEERANGE_TEST_EXE}")
    add_test(NAME meleerange_guard COMMAND meleerange_patch_test "${MELEERANGE_TEST_EXE}")
else()
    add_test(NAME meleerange_guard COMMAND meleerange_patch_test)
endif()

add_executable(hpmp_patch_test hpmp_patch_test.cpp)
set(HPMP_TEST_EXE "Z:/MSv83/SlumsMS-dev/client-1.5.0-fixes2/exe-copy/MapleStory.exe" CACHE FILEPATH
    "MapleStory.exe whose bytes the 4-byte HP/MP test checks (optional)")
if(HPMP_TEST_EXE AND EXISTS "${HPMP_TEST_EXE}")
    add_test(NAME hpmp_sites COMMAND hpmp_patch_test "${HPMP_TEST_EXE}")
else()
    add_test(NAME hpmp_sites COMMAND hpmp_patch_test)
endif()

add_executable(d3d9host_policy_test d3d9host_policy_test.cpp)
add_test(NAME d3d9host_adapter_fallback COMMAND d3d9host_policy_test)

add_executable(customwz_policy_test customwz_policy_test.cpp)
add_test(NAME customwz_image_prefix COMMAND customwz_policy_test)
add_executable(overlay_policy_test overlay_policy_test.cpp)
add_test(NAME overlay_hud_layout COMMAND overlay_policy_test)

add_executable(uilayout_policy_test uilayout_policy_test.cpp)
add_test(NAME uilayout_move_stock_ui COMMAND uilayout_policy_test)

add_executable(uitour_policy_test uitour_policy_test.cpp)
add_test(NAME uitour_first_run COMMAND uitour_policy_test)

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

if(MAPLEROOT_WEATHER AND EXISTS "${MOVEMENTKEYS_TEST_CLIENT}/Data/Map/Obj/connect.img")
    add_executable(weather_rope_clear_test weather_rope_clear_test.cpp ../src/weathercanvas.cpp ../src/debug.cpp)
    target_link_libraries(weather_rope_clear_test PRIVATE WzLib)
    target_include_directories(weather_rope_clear_test PRIVATE ../src)
    add_test(NAME weather_rope_sway_no_box COMMAND weather_rope_clear_test "${MOVEMENTKEYS_TEST_CLIENT}")
endif()

if(EXISTS "${MOVEMENTKEYS_TEST_CLIENT}/Data/Map/Tile/allInvisibleTile.img")
    add_executable(inlink_guard_test inlink_guard_test.cpp ../src/inlink_guard.cpp ../src/debug.cpp)
    target_link_libraries(inlink_guard_test PRIVATE WzLib)
    target_include_directories(inlink_guard_test PRIVATE ../src)
    add_test(NAME inlink_broken_link_no_crash COMMAND inlink_guard_test "${MOVEMENTKEYS_TEST_CLIENT}")
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
    add_executable(coloringprism_text_test coloringprism_text_test.cpp)
    add_test(NAME colorprism_item_text COMMAND coloringprism_text_test)
    add_executable(coloringprism_layout_test coloringprism_layout_test.cpp)
    add_test(NAME colorprism_window_layout COMMAND coloringprism_layout_test)
    add_executable(weapontint_walk_test weapontint_walk_test.cpp)
    add_test(NAME colorprism_swap_walk COMMAND weapontint_walk_test)
endif()

if(MAPLEROOT_ATTACK_DIAG)
    add_executable(attackdiag_rules_test attackdiag_rules_test.cpp)
    add_test(NAME attackdiag_remote_attack_rules COMMAND attackdiag_rules_test)
endif()

add_executable(attackstop_patch_test attackstop_patch_test.cpp)
add_test(NAME attackstop_moveattack_sites COMMAND attackstop_patch_test)

add_executable(ui_wheel_test ui_wheel_test.cpp)
add_test(NAME ui_wheel_direction COMMAND ui_wheel_test)

add_executable(bounce_rules_test bounce_rules_test.cpp)
add_test(NAME bounce_doubletap_and_zones COMMAND bounce_rules_test)

add_executable(hud_meters_policy_test hud_meters_policy_test.cpp)
add_test(NAME hud_meters_exp_dps COMMAND hud_meters_policy_test)
add_executable(window_policy_test window_policy_test.cpp)
add_test(NAME window_borderless_place COMMAND window_policy_test)
add_executable(display_policy_test display_policy_test.cpp)
add_test(NAME display_toggle_rules COMMAND display_policy_test)
add_executable(midair_patch_test midair_patch_test.cpp)
add_test(NAME midair_sites_policy COMMAND midair_patch_test)
add_executable(damagefmt_policy_test damagefmt_policy_test.cpp)
add_test(NAME damagefmt_short_numbers COMMAND damagefmt_policy_test)
add_executable(crash_report_test crash_report_test.cpp)
add_test(NAME crash_report_classify_scrub COMMAND crash_report_test)
