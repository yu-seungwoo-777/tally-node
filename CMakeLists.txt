cmake_minimum_required(VERSION 3.16.0)

# ============================================================================
# 레이어별 컴포넌트 디렉토리 등록 (5계층 아키텍처)
# ============================================================================
# 중요: include($ENV{IDF_PATH}/tools/cmake/project.cmake) 이전에 설정
# 중요: set 대신 list(APPEND) 사용 - PlatformIO도 EXTRA_COMPONENT_DIRS를 사용하므로
# 의존성 방향: 낮은 번호 → 높은 번호 (상향 의존)
# 01_app → 02 → 03 → 04 → 05
# 00_common은 모든 레이어에서 접근 가능

# 1. 레이어 폴더 등록
list(APPEND EXTRA_COMPONENT_DIRS
    "${CMAKE_SOURCE_DIR}/components/00_common"
    "${CMAKE_SOURCE_DIR}/components/01_app"
    "${CMAKE_SOURCE_DIR}/components/02_presentation"
    "${CMAKE_SOURCE_DIR}/components/03_service"
    "${CMAKE_SOURCE_DIR}/components/04_driver"
    "${CMAKE_SOURCE_DIR}/components/05_hal"
)

# 2. 하위 컴포넌트 자동 등록 함수
function(register_subcomponents layer_dir result_var)
    file(GLOB subdirs LIST_DIRECTORIES true "${layer_dir}/*")
    set(subcomps "")
    foreach(subdir ${subdirs})
        if(IS_DIRECTORY "${subdir}" AND EXISTS "${subdir}/CMakeLists.txt")
            list(APPEND subcomps "${subdir}")
        endif()
    endforeach()
    set(${result_var} ${subcomps} PARENT_SCOPE)
endfunction()

# 3. 각 레이어의 하위 컴포넌트 등록 (레이어 내 CMakeLists.txt는 제외)
register_subcomponents("${CMAKE_SOURCE_DIR}/components/00_common" common_subcomps)
register_subcomponents("${CMAKE_SOURCE_DIR}/components/01_app" app_subcomps)
register_subcomponents("${CMAKE_SOURCE_DIR}/components/02_presentation" presentation_subcomps)
register_subcomponents("${CMAKE_SOURCE_DIR}/components/03_service" service_subcomps)
register_subcomponents("${CMAKE_SOURCE_DIR}/components/04_driver" driver_subcomps)
register_subcomponents("${CMAKE_SOURCE_DIR}/components/05_hal" hal_subcomps)

# 4. 하위 컴포넌트들을 EXTRA_COMPONENT_DIRS에 추가
list(APPEND EXTRA_COMPONENT_DIRS
    ${common_subcomps}
    ${app_subcomps}
    ${presentation_subcomps}
    ${service_subcomps}
    ${driver_subcomps}
    ${hal_subcomps}
)

# ESP-IDF 프로젝트 시작
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(tally_node)
