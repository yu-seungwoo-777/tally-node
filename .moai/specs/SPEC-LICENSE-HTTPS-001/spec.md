# SPEC-LICENSE-HTTPS-001: 라이센스 인증 시스템 HTTPS 지원

## 메타데이터

| 항목 | 값 |
|------|-----|
| SPEC ID | SPEC-LICENSE-HTTPS-001 |
| 제목 | 라이센스 인증 시스템 HTTPS 지원 |
| 상태 | Planned |
| 우선순위 | High (보안 필수) |
| 생성일 | 2026-01-21 |
| 담당 에이전트 | expert-backend |
| 관련 파일 | license_client.cpp, license_client.h, license_service.cpp |

---

## 1. 환경 (Environment)

### 1.1 시스템 환경

- **하드웨어**: ESP32-S3 기반 EORA Tally 시스템
- **빌드 시스템**: PlatformIO + ESP-IDF 5.5.0
- **아키텍처**: 5계층 구조 (00_common -> 05_hal)
- **메모리 제약**: ESP32-S3 RAM 제한 고려 필요

### 1.2 현재 상태

- **프로토콜**: HTTP (암호화 없음)
- **서버 URL**: `http://tally-node.duckdns.org/api/validate-license`
- **HTTP 클라이언트**: ESP-IDF `esp_http_client`
- **타임아웃**: 15초 (LICENSE_TIMEOUT_MS)

### 1.3 보안 취약점

| 취약점 | 위험도 | 설명 |
|--------|--------|------|
| 라이센스 키 평문 전송 | Critical | 네트워크 스니핑으로 키 탈취 가능 |
| MAC 주소 평문 전송 | High | 디바이스 식별 정보 노출 |
| API 키 평문 전송 | Critical | 인증 우회 가능 |

---

## 2. 가정 (Assumptions)

### 2.1 기술적 가정

- [A-01] 라이센스 서버 (`tally-node.duckdns.org`)가 HTTPS를 지원함
- [A-02] 서버 인증서가 유효한 CA에서 발급됨 (Let's Encrypt 등)
- [A-03] ESP-IDF 5.5.0의 mbedTLS가 TLS 1.2/1.3을 지원함
- [A-04] ESP32-S3에 인증서 번들 저장을 위한 충분한 플래시 공간이 있음

### 2.2 운영적 가정

- [A-05] 인증서 만료 시 OTA 업데이트 또는 번들 업데이트 가능
- [A-06] 네트워크 환경이 443 포트 HTTPS 트래픽을 허용함
- [A-07] DNS 해석이 정상 작동함

### 2.3 검증 필요 가정

| 가정 | 신뢰도 | 검증 방법 |
|------|--------|----------|
| A-01 | Medium | curl -I https://tally-node.duckdns.org 테스트 |
| A-04 | High | 플래시 파티션 테이블 확인 |

---

## 3. 요구사항 (Requirements)

### 3.1 기능 요구사항

#### FR-01: HTTPS 프로토콜 지원 (Ubiquitous)

> 시스템은 **항상** 라이센스 서버와 HTTPS를 통해 암호화된 통신을 수행해야 한다.

- TLS 1.2 이상 사용
- 서버 인증서 검증 수행
- 암호화된 채널을 통해 라이센스 키, MAC 주소, API 키 전송

#### FR-02: 인증서 검증 (Event-Driven)

> **WHEN** HTTPS 연결 수립 시 **THEN** 서버 인증서를 ESP x509 Certificate Bundle로 검증해야 한다.

- ESP-IDF의 `crt_bundle_attach` 사용
- 인증서 체인 검증 수행
- 신뢰할 수 없는 인증서 거부

#### FR-03: SSL 오류 처리 (State-Driven)

> **IF** SSL 핸드셰이크 실패 **THEN** 적절한 오류 코드와 메시지를 반환해야 한다.

- SSL 오류 코드 분류 및 로깅
- 사용자 친화적 오류 메시지 제공
- 재시도 로직 지원

#### FR-04: HTTP 폴백 금지 (Unwanted)

> 시스템은 HTTPS 실패 시 **HTTP로 폴백하지 않아야 한다**.

- HTTP 프로토콜 완전 제거
- 보안 저하 방지

### 3.2 비기능 요구사항

#### NFR-01: 성능

- HTTPS 연결 수립: 최대 5초 (TLS 핸드셰이크 포함)
- 전체 라이센스 검증: 최대 20초 (기존 15초 + TLS 오버헤드)

#### NFR-02: 메모리

- 추가 힙 사용량: 최대 20KB (SSL 컨텍스트)
- 플래시 사용량: 최대 50KB (인증서 번들)

#### NFR-03: 호환성

- ESP-IDF 5.5.0 완전 호환
- 기존 API 인터페이스 유지

---

## 4. 사양 (Specifications)

### 4.1 수정 대상 파일

| 파일 | 변경 유형 | 설명 |
|------|----------|------|
| `license_client.h` | 수정 | URL을 HTTPS로 변경, 인증서 관련 상수 추가 |
| `license_client.cpp` | 수정 | SSL 설정 추가, 오류 처리 개선 |
| `CMakeLists.txt` | 수정 | mbedTLS 및 인증서 번들 의존성 추가 |

### 4.2 HTTP 클라이언트 설정 변경

**현재 설정:**
```c
esp_http_client_config_t config = {};
config.url = url;  // http://...
config.method = HTTP_METHOD_POST;
config.timeout_ms = LICENSE_TIMEOUT_MS;
```

**목표 설정:**
```c
esp_http_client_config_t config = {};
config.url = url;  // https://...
config.method = HTTP_METHOD_POST;
config.timeout_ms = LICENSE_HTTPS_TIMEOUT_MS;
config.transport_type = HTTP_TRANSPORT_OVER_SSL;
config.crt_bundle_attach = esp_crt_bundle_attach;  // CA 번들 사용
config.skip_cert_common_name_check = false;        // CN 검증 활성화
```

### 4.3 상수 정의 변경

```c
// 기존
#define LICENSE_SERVER_BASE     "http://tally-node.duckdns.org"

// 변경
#define LICENSE_SERVER_BASE     "https://tally-node.duckdns.org"
#define LICENSE_HTTPS_TIMEOUT_MS 20000  // TLS 오버헤드 고려
```

### 4.4 오류 코드 확장

```c
typedef enum {
    LICENSE_ERR_NONE = 0,
    LICENSE_ERR_NO_WIFI,
    LICENSE_ERR_CONNECTION_FAILED,
    LICENSE_ERR_SSL_HANDSHAKE,      // 신규: SSL 핸드셰이크 실패
    LICENSE_ERR_CERT_INVALID,       // 신규: 인증서 검증 실패
    LICENSE_ERR_CERT_EXPIRED,       // 신규: 인증서 만료
    LICENSE_ERR_JSON_PARSE,
    LICENSE_ERR_SERVER_ERROR,
} license_error_t;
```

### 4.5 의존성 추가 (CMakeLists.txt)

```cmake
idf_component_register(
    SRCS "license_client.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_client esp_tls mbedtls esp_crt_bundle
)
```

---

## 5. 추적성 (Traceability)

### 5.1 요구사항-사양 매핑

| 요구사항 | 사양 | 검증 방법 |
|----------|------|----------|
| FR-01 | 4.2, 4.3 | 단위 테스트, 네트워크 캡처 |
| FR-02 | 4.2, 4.5 | 인증서 검증 테스트 |
| FR-03 | 4.4 | 오류 주입 테스트 |
| FR-04 | 4.3 | 코드 리뷰, 정적 분석 |

### 5.2 관련 문서

| 문서 | 참조 |
|------|------|
| ESP-IDF HTTPS Client | https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32s3/api-reference/protocols/esp_http_client.html |
| ESP x509 Certificate Bundle | https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32s3/api-reference/protocols/esp_crt_bundle.html |
| ESP-TLS | https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32s3/api-reference/protocols/esp_tls.html |

---

## 6. 태그

`#security` `#https` `#tls` `#license` `#esp-idf` `#critical`
