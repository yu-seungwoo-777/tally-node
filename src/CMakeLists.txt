# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)

# DEVICE_MODE 변수 확인 (platformio.ini의 cmake_extra_args에서 전달됨)
set(IS_RX_MODE FALSE)
if(DEFINED DEVICE_MODE)
    message("========== DEVICE_MODE: ${DEVICE_MODE} ==========")
    if("${DEVICE_MODE}" STREQUAL "RX")
        set(IS_RX_MODE TRUE)
    endif()
else()
    # Fallback: CMAKE_CXX_FLAGS 확인
    if("${CMAKE_C_FLAGS}" MATCHES "DEVICE_MODE_RX" OR
       "${CMAKE_CXX_FLAGS}" MATCHES "DEVICE_MODE_RX")
        message("========== RX MODE DETECTED (from FLAGS) ==========")
        set(IS_RX_MODE TRUE)
    else()
        message("========== TX MODE DETECTED (default) ==========")
    endif()
endif()

# 모드별 의존성 설정
set(COMMON_REQUIRES
    simple_log
    info
    system
    lora
    interface_cli
    display
)

# TX 모드 전용 컴포넌트
if(NOT IS_RX_MODE)
    message("========== Including TX components ==========")
    list(APPEND COMMON_REQUIRES
        network
        interface_web
        switcher
    )
else()
    message("========== Excluding TX components ==========")
endif()

idf_component_register(
    SRCS ${app_sources}
    REQUIRES ${COMMON_REQUIRES}
)
