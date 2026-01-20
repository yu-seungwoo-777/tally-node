idf_component_register(
    SRCS "license_client.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_client json t_log esp_netif esp_tls mbedtls esp_crt_bundle
)
