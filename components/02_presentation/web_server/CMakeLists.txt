# web_server CMakeLists.txt

idf_component_register(
    SRCS "web_server.cpp"
    INCLUDE_DIRS "include" "static_embed"
    REQUIRES esp_http_server esp_event json event_bus lora_protocol esp_http_client esp_netif license_service license_client esp_timer esp_crt_bundle
)
