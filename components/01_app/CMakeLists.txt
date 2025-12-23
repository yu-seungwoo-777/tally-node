# 01_app Layer CMakeLists.txt

idf_component_register(
    SRCS "lora_test/lora_test_app.cpp"
    INCLUDE_DIRS "lora_test/include"
    REQUIRES
        lora_service
        event_bus
)
