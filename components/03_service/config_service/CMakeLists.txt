idf_component_register(
    SRCS "config_service.cpp"
    INCLUDE_DIRS "include"
    REQUIRES nvs_flash t_log event_bus license_service
)

# 컴포넌트 별칭 설정 (대소문자 구분 문제 해결)
set(config_service_CONFIG_SERVICE CONFIG_SERVICE)
