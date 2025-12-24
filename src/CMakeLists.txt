# 메인 컴포넌트
# Tally TX/RX 앱 또는 이더넷 테스트 모드

idf_component_register(
    SRCS "main.cpp"
    REQUIRES lora_test network_test ethernet_hal nvs_flash esp_netif
)
