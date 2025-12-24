# Display HAL Component (I2C + U8g2)

# U8g2 라이브러리 소스
file(GLOB U8G2_SOURCES
    u8g2_lib/csrc/*.c
)

# U8g2 ESP-IDF HAL 소스
file(GLOB U8G2_HAL_SOURCES
    u8g2_hal/src/*.c
)

# Display HAL 소스
set(DISPLAY_HAL_SOURCES
    display_hal.c
)

# 모든 소스 합치기
set(SOURCES ${U8G2_SOURCES} ${U8G2_HAL_SOURCES} ${DISPLAY_HAL_SOURCES})

idf_component_register(
    SRCS ${SOURCES}
    INCLUDE_DIRS
        "include"
        "u8g2_lib/csrc"
        "u8g2_hal/include"
    REQUIRES driver esp_common t_log
)
