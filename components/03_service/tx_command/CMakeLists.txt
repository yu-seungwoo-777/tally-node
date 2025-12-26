idf_component_register(
    SRCS "tx_command.cpp"
    INCLUDE_DIRS "include"
    REQUIRES lora_protocol lora_service t_log
)
