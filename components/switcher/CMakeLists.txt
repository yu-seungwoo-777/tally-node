# TX 모드에서만 빌드
if("${CMAKE_CXX_FLAGS}" MATCHES "DEVICE_MODE_RX")
    # RX 모드: 빈 컴포넌트 등록
    idf_component_register(
        INCLUDE_DIRS "."
    )
    message("Switcher component DISABLED (RX mode)")
else()
    # TX 모드: 정상 빌드
    idf_component_register(
        SRCS
            "common/platform/sw_platform_esp.c"
            "handler/src/switcher.c"
            "protocol/atem/src/atem_client.c"
            "protocol/atem/src/atem_debug.c"
            "protocol/atem/src/atem_parser.c"
            "protocol/obs/src/obs_client.c"
            "protocol/obs/src/obs_base64.c"
            "protocol/obs/src/obs_json.c"
            "protocol/obs/src/obs_sha256.c"
            "protocol/obs/src/obs_websocket.c"
            "protocol/vmix/src/vmix_client.c"
            "manager/src/SwitcherManager.cpp"
        INCLUDE_DIRS
            "include"
            "handler/include"
            "common/include"
            "protocol/atem/include"
            "protocol/obs/include"
            "protocol/vmix/include"
            "manager/include"
        REQUIRES system lwip esp_timer simple_log
    )
    message("Switcher component ENABLED (TX mode)")
endif()
