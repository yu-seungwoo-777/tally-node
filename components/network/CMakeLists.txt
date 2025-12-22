# TX 모드에서만 빌드
if("${CMAKE_CXX_FLAGS}" MATCHES "DEVICE_MODE_RX")
    # RX 모드: 빈 컴포넌트 등록
    idf_component_register(
        INCLUDE_DIRS "."
    )
    message("Network component DISABLED (RX mode)")
else()
    # TX 모드: 정상 빌드
    idf_component_register(
        SRCS
            "core/WiFiCore.cpp"
            "core/EthernetCore.cpp"
            "manager/NetworkManager.cpp"
        INCLUDE_DIRS
            "core"
            "manager"
        REQUIRES system esp_wifi esp_eth esp_netif simple_log
    )
    message("Network component ENABLED (TX mode)")
endif()
