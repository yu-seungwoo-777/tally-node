# web_server CMakeLists.txt

idf_component_register(
    SRCS "web_server.cpp"
    INCLUDE_DIRS "include" "static_embed"
    REQUIRES esp_http_server esp_event json
    PRIV_REQUIRES config_service lora_service network_service switcher_service
)
