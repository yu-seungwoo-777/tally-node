idf_component_register(
    SRCS "lora_driver.cpp"
    INCLUDE_DIRS "include"
    REQUIRES lora_hal driver esp_timer freertos
)
