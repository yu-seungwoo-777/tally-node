# web_resources.c가 없으면 생성
# 주의: HTML/CSS/JS 수정 후 빌드하기 전에 수동으로 실행하세요:
#   python3 components/webserver/convert_web_resources.py components/webserver/www components/webserver/src/web_resources.c

# TX 모드에서만 빌드
if("${CMAKE_CXX_FLAGS}" MATCHES "DEVICE_MODE_RX")
    # RX 모드: 빈 컴포넌트 등록
    idf_component_register()
    message("WebServer component DISABLED (RX mode)")
else()
    # TX 모드: 정상 빌드
    idf_component_register(
        SRCS
            "src/WebServerCore.cpp"
            "src/ApiHandler.cpp"
            "src/MonitorApi.cpp"
            "src/web_resources.c"
        INCLUDE_DIRS "include" "src"
        REQUIRES esp_http_server simple_log info
        PRIV_REQUIRES system network json lora
    )

    # ESP-IDF 컴포넌트 시스템에서 빌드 순서 지정
    # PRIV_REQUIRES에 순서대로 나열하여 의존성 보장
    message("WebServer component ENABLED (TX mode)")
endif()
