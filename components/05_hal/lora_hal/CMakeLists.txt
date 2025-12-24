idf_component_register(
    SRCS "lora_hal.cpp"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_timer freertos t_log
)

# C++ 컴파일 옵션
target_compile_options(${COMPONENT_LIB} PRIVATE
    -std=c++17
    -fno-exceptions
    -fno-rtti
)
