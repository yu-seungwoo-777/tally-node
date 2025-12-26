# web_server CMakeLists.txt

idf_component_register(
    SRCS "web_server.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_server esp_event event_bus
)
