# LoRa 테스트용 메인

idf_component_register(
    SRCS "main.cpp"
    REQUIRES lora_test nvs_flash
)
