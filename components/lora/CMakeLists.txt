# LoRa Component - 4계층 아키텍처 기반 재구성

# Include 디렉토리 설정
set(COMPONENT_ADD_INCLUDEDIRS
    include
    include/common
    include/realtime
    include/manager
    include/protocol
    include/service
    include/core
)

# 소스 파일 검색
file(GLOB_RECURSE COMPONENT_SRCS
    "src/*.c"
    "src/*.cpp"
)

# 컴포넌트 등록
idf_component_register(
    SRCS ${COMPONENT_SRCS}
    INCLUDE_DIRS ${COMPONENT_ADD_INCLUDEDIRS}
    REQUIRES system switcher led esp_timer spi_flash driver simple_log
)

# 빌드 시 정보 출력
message(STATUS "LoRa component configured with 4-layer architecture")
message(STATUS "  - Core Layer: Hardware abstraction")
message(STATUS "  - Manager Layer: LoRa communication management")
message(STATUS "  - Service Layer: Tally dispatch and device management")
message(STATUS "  - Realtime Layer: High-performance mapping")