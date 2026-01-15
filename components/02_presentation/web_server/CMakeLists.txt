# web_server CMakeLists.txt

idf_component_register(
    SRCS
        "web_server.cpp"
        "web_server_routes.cpp"
        "web_server_cache.cpp"
        "web_server_events.cpp"
        "web_server_json.cpp"
        "web_server_config.cpp"
        "handlers/api_status.cpp"
        "handlers/api_config.cpp"
        "handlers/api_devices.cpp"
        "handlers/api_lora.cpp"
        "handlers/api_test.cpp"
        "handlers/api_led.cpp"
        "handlers/api_license.cpp"
        "handlers/api_notices.cpp"
        "handlers/api_static.cpp"
    INCLUDE_DIRS "include" "static_embed" "."
    REQUIRES t_log app_types esp_http_server esp_event json event_bus lora_protocol esp_http_client esp_netif esp_timer
)
