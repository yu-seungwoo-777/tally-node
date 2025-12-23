idf_component_register(
    SRCS "EthernetHal.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_eth esp_netif esp_event freertos
)
