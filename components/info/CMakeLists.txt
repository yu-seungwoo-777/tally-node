# InfoManager Component
# 시스템 정보 중앙 관리 컴포넌트

idf_component_register(
    SRCS
        "src/InfoManager.cpp"
        "src/DeviceIdManager.cpp"
        "src/info_manager_c.cpp"
    INCLUDE_DIRS
        "include"
    PRIV_INCLUDE_DIRS
        "src"
    REQUIRES
        nvs_flash
        esp_netif
        esp_wifi
        simple_log
        esp_timer
)

# C++ 설정
target_compile_options(${COMPONENT_LIB} PRIVATE
    -std=c++17
    -fno-exceptions
    -fno-rtti
)

# Release 빌드 시 최적화
target_compile_options(${COMPONENT_LIB} PRIVATE
    $<$<CONFIG:Release>:-O2>
    $<$<CONFIG:Release>:-flto>
)