idf_component_register(
    SRCS
        "config/ConfigCore.cpp"
        "monitor/SystemMonitor.cpp"
        "button_poll/core/src/button_poll.c"
        "button_poll/core/src/button_actions.c"
    INCLUDE_DIRS
        "common"
        "config"
        "monitor"
        "button_poll/core/include"
    REQUIRES nvs_flash switcher json esp_adc esp_timer esp_driver_tsens driver simple_log display info
)
