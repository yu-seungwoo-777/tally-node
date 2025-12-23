# 메인 컴포넌트
# lora_test 또는 network_test 앱을 선택하여 사용

idf_component_register(
    SRCS "main.cpp"
    REQUIRES lora_test network_test nvs_flash
)
