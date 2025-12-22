idf_component_register(
    SRCS "LoRaCore.cpp" "lora_core_c.cpp"
    INCLUDE_DIRS "include"
    REQUIRES RadioLib
)