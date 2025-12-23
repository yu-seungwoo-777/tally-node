idf_component_register(
    SRCS "EthernetDriver.cpp"
    INCLUDE_DIRS "include"
    REQUIRES ethernet_hal esp_eth esp_netif esp_event freertos
)
