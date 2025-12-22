# Display Manager Component

# u8g2 소스 파일 직접 포함
file(GLOB U8G2_SOURCES
    u8g2_lib/csrc/*.c
    u8g2-hal-esp-idf/src/*.c
)

# 아이콘 소스 파일
file(GLOB ICON_SOURCES
    src/ui/*.c
    icons/*.c
)

# Display Manager 소스 (하위 폴더 포함)
file(GLOB_RECURSE DISPLAY_SOURCES src/*)

# 수동으로 각 폴더의 소스 지정 (안정성 확보)
file(GLOB CORE_SOURCES src/core/*.c src/core/*.cpp)
file(GLOB UI_SOURCES src/ui/*.c src/ui/*.cpp)
file(GLOB BUFFER_SOURCES src/buffer/*.c src/buffer/*.cpp)

# 페이지 소스 - 모두 포함 (TxPage.cpp는 DEVICE_MODE_TX 조건으로 처리)
file(GLOB PAGES_SOURCES
    src/pages/BootScreen.c
    src/pages/SettingsPage.cpp
    src/pages/RxPage.cpp
    src/pages/TxPage.cpp
)

list(APPEND DISPLAY_SOURCES ${CORE_SOURCES} ${PAGES_SOURCES} ${UI_SOURCES} ${BUFFER_SOURCES})

# 모든 소스 합치기
set(SOURCES ${DISPLAY_SOURCES} ${U8G2_SOURCES} ${ICON_SOURCES})

idf_component_register(
    SRCS ${SOURCES}
    INCLUDE_DIRS
        "include"
        "include/core"
        "include/pages"
        "include/ui"
        "include/buffer"
        "u8g2_lib/csrc"
        "u8g2-hal-esp-idf/include"
    REQUIRES driver system lora info
)