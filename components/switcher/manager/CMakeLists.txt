# Switcher Manager - TX 모드에서만 빌드
if(DEFINED DEVICE_MODE AND DEVICE_MODE STREQUAL "TX")
    idf_component_register(
        SRCS "src/SwitcherManager.cpp"
        INCLUDE_DIRS "include"
        REQUIRES system switcher simple_log
    )
    message("SwitcherManager component ENABLED (TX mode)")
else()
    # RX 모드: 컴포넌트를 빈 것으로 등록
    idf_component_register()
    message("SwitcherManager component DISABLED (RX mode)")
endif()