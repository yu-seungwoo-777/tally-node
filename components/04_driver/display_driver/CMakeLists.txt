# Display Driver Component (C++)

# Display 드라이버 소스
file(GLOB DISPLAY_DRIVER_SOURCES
    *.cpp
)

idf_component_register(
    SRCS ${DISPLAY_DRIVER_SOURCES}
    INCLUDE_DIRS "include"
    REQUIRES display_hal t_log
)
