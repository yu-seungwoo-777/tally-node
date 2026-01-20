# SPEC-LICENSE-HTTPS-001: 구현 계획

## 관련 SPEC

- **SPEC ID**: SPEC-LICENSE-HTTPS-001
- **제목**: 라이센스 인증 시스템 HTTPS 지원

---

## 1. 마일스톤

### 마일스톤 1: 서버 HTTPS 지원 확인 (Priority: High)

**목표**: 라이센스 서버의 HTTPS 지원 여부 검증

**작업 항목**:
- [ ] 서버 HTTPS 엔드포인트 접근성 테스트
- [ ] SSL 인증서 유효성 확인 (발급 기관, 만료일, CN)
- [ ] TLS 버전 및 암호화 스위트 확인

**검증 기준**:
- `curl -I https://tally-node.duckdns.org` 성공
- 인증서가 신뢰할 수 있는 CA에서 발급됨

**의존성**: 없음

---

### 마일스톤 2: ESP-IDF 설정 및 의존성 구성 (Priority: High)

**목표**: HTTPS 통신에 필요한 ESP-IDF 컴포넌트 구성

**작업 항목**:
- [ ] `sdkconfig`에서 mbedTLS 및 인증서 번들 활성화
- [ ] `CMakeLists.txt`에 필요한 컴포넌트 의존성 추가
- [ ] 플래시 파티션 크기 검토 (인증서 번들 공간 확보)

**필요한 sdkconfig 설정**:
```
CONFIG_ESP_TLS_USING_MBEDTLS=y
CONFIG_ESP_TLS_SERVER=n
CONFIG_MBEDTLS_SSL_PROTO_TLS1_2=y
CONFIG_MBEDTLS_CERTIFICATE_BUNDLE=y
CONFIG_MBEDTLS_CERTIFICATE_BUNDLE_DEFAULT_FULL=y
```

**검증 기준**:
- 빌드 성공
- 플래시 사용량 50KB 이하 증가

**의존성**: 마일스톤 1

---

### 마일스톤 3: license_client.h 수정 (Priority: High)

**목표**: 헤더 파일에 HTTPS 관련 상수 및 타입 정의

**작업 항목**:
- [ ] `LICENSE_SERVER_BASE`를 HTTPS URL로 변경
- [ ] `LICENSE_HTTPS_TIMEOUT_MS` 상수 추가 (20초)
- [ ] SSL 관련 오류 열거형 추가
- [ ] API 문서 주석 업데이트

**코드 변경 예시**:
```c
// URL 변경
#define LICENSE_SERVER_BASE     "https://tally-node.duckdns.org"

// 타임아웃 증가 (TLS 핸드셰이크 고려)
#define LICENSE_HTTPS_TIMEOUT_MS 20000
```

**검증 기준**:
- 컴파일 성공
- 기존 API 시그니처 유지

**의존성**: 마일스톤 2

---

### 마일스톤 4: license_client.cpp SSL 구현 (Priority: High)

**목표**: HTTP 클라이언트에 SSL/TLS 설정 추가

**작업 항목**:
- [ ] `esp_crt_bundle_attach` 포함
- [ ] `http_post()` 함수에 SSL 설정 추가
- [ ] `license_client_connection_test()`에 SSL 설정 추가
- [ ] SSL 오류 처리 로직 구현
- [ ] 상세 오류 로깅 추가

**핵심 코드 변경**:
```c
#include "esp_crt_bundle.h"

// http_post() 함수 내
esp_http_client_config_t config = {};
config.url = url;
config.method = HTTP_METHOD_POST;
config.timeout_ms = LICENSE_HTTPS_TIMEOUT_MS;
config.transport_type = HTTP_TRANSPORT_OVER_SSL;
config.crt_bundle_attach = esp_crt_bundle_attach;
config.skip_cert_common_name_check = false;
// ... 기존 설정 유지
```

**검증 기준**:
- HTTPS 연결 성공
- 인증서 검증 동작
- 오류 시 적절한 로그 출력

**의존성**: 마일스톤 3

---

### 마일스톤 5: 오류 처리 강화 (Priority: Medium)

**목표**: SSL 관련 오류에 대한 상세 처리 구현

**작업 항목**:
- [ ] SSL 핸드셰이크 실패 처리
- [ ] 인증서 검증 실패 처리
- [ ] 인증서 만료 처리
- [ ] 네트워크 타임아웃 처리 개선
- [ ] 오류 메시지 국제화 준비

**오류 매핑**:
| ESP 오류 코드 | 사용자 메시지 |
|--------------|---------------|
| ESP_ERR_ESP_TLS_CANNOT_RESOLVE_HOSTNAME | DNS 해석 실패 |
| ESP_ERR_ESP_TLS_FAILED_CONNECT_TO_HOST | 서버 연결 실패 |
| ESP_ERR_MBEDTLS_SSL_HANDSHAKE_FAILED | SSL 핸드셰이크 실패 |
| ESP_ERR_MBEDTLS_X509_CERT_VERIFY_FAILED | 인증서 검증 실패 |

**검증 기준**:
- 각 오류 시나리오에 대한 적절한 메시지 출력
- 오류 코드별 분류 로깅

**의존성**: 마일스톤 4

---

### 마일스톤 6: 통합 테스트 및 검증 (Priority: High)

**목표**: 전체 HTTPS 라이센스 검증 흐름 테스트

**작업 항목**:
- [ ] 정상 라이센스 검증 테스트
- [ ] 잘못된 라이센스 키 테스트
- [ ] 네트워크 오류 시뮬레이션
- [ ] 메모리 사용량 측정
- [ ] 성능 벤치마크 (응답 시간)

**테스트 시나리오**:
1. 유효한 라이센스 키로 검증 요청 -> 성공
2. 무효한 라이센스 키로 검증 요청 -> 실패 (적절한 오류)
3. WiFi 연결 없이 요청 -> 연결 오류
4. 서버 다운 시 요청 -> 타임아웃 오류

**검증 기준**:
- 모든 테스트 시나리오 통과
- 힙 사용량 증가 20KB 이하
- 평균 응답 시간 20초 이하

**의존성**: 마일스톤 5

---

## 2. 기술적 접근

### 2.1 인증서 처리 방식

**선택: ESP x509 Certificate Bundle (권장)**

| 방식 | 장점 | 단점 |
|------|------|------|
| `cert_pem` (직접 PEM) | 최소 플래시 사용 | 인증서 갱신 시 펌웨어 업데이트 필요 |
| `crt_bundle_attach` (CA 번들) | 자동 CA 검증, 유연성 | 플래시 약 50KB 사용 |

**결정 근거**:
- Let's Encrypt 등 일반 CA 인증서 자동 지원
- 인증서 갱신 시 펌웨어 업데이트 불필요
- ESP-IDF 공식 권장 방식

### 2.2 메모리 최적화

- SSL 버퍼 크기 최소화: 기본값 대신 필요한 만큼만 할당
- 연결 종료 시 즉시 SSL 컨텍스트 해제
- 단일 연결 사용 (keep-alive 활용)

### 2.3 보안 강화 옵션 (Optional Goal)

- API 키를 NVS에 암호화 저장 (현재 하드코딩 개선)
- 인증서 피닝 (Certificate Pinning) 고려
- mTLS (상호 TLS 인증) 향후 고려

---

## 3. 아키텍처 설계

### 3.1 컴포넌트 다이어그램

```
┌─────────────────────────────────────────────────────────┐
│                    03_service Layer                      │
│  ┌─────────────────────────────────────────────────┐    │
│  │              license_service.cpp                 │    │
│  │  - 라이센스 검증 로직                            │    │
│  │  - WiFi 연결 상태 확인                           │    │
│  └─────────────────────┬───────────────────────────┘    │
└────────────────────────│────────────────────────────────┘
                         │ API 호출
                         ▼
┌─────────────────────────────────────────────────────────┐
│                    04_driver Layer                       │
│  ┌─────────────────────────────────────────────────┐    │
│  │              license_client.cpp                  │    │
│  │  - HTTPS 클라이언트 (수정 대상)                  │    │
│  │  - SSL/TLS 설정                                  │    │
│  │  - 인증서 번들 연결                              │    │
│  └─────────────────────┬───────────────────────────┘    │
└────────────────────────│────────────────────────────────┘
                         │ ESP-IDF API
                         ▼
┌─────────────────────────────────────────────────────────┐
│                    ESP-IDF Framework                     │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │esp_http_client│  │   esp_tls   │  │   mbedTLS    │  │
│  │              │  │              │  │              │  │
│  │ HTTP/HTTPS   │──│ TLS 핸들링   │──│ 암호화/인증  │  │
│  └──────────────┘  └──────────────┘  └──────────────┘  │
│                           │                             │
│                    ┌──────┴──────┐                      │
│                    │ Certificate │                      │
│                    │   Bundle    │                      │
│                    └─────────────┘                      │
└─────────────────────────────────────────────────────────┘
```

### 3.2 시퀀스 다이어그램 (HTTPS 라이센스 검증)

```
license_service     license_client      esp_http_client     esp_tls/mbedTLS     Server
      │                   │                   │                   │                │
      │ validate()        │                   │                   │                │
      │──────────────────>│                   │                   │                │
      │                   │ init(SSL config)  │                   │                │
      │                   │──────────────────>│                   │                │
      │                   │                   │ TLS handshake     │                │
      │                   │                   │──────────────────>│                │
      │                   │                   │                   │ ClientHello    │
      │                   │                   │                   │───────────────>│
      │                   │                   │                   │ ServerHello+   │
      │                   │                   │                   │ Certificate    │
      │                   │                   │                   │<───────────────│
      │                   │                   │ verify cert       │                │
      │                   │                   │<──────────────────│                │
      │                   │                   │ handshake done    │                │
      │                   │                   │<──────────────────│                │
      │                   │ perform()         │                   │                │
      │                   │──────────────────>│                   │                │
      │                   │                   │ encrypted POST    │                │
      │                   │                   │───────────────────────────────────>│
      │                   │                   │ encrypted response│                │
      │                   │                   │<───────────────────────────────────│
      │                   │ response          │                   │                │
      │                   │<──────────────────│                   │                │
      │ result            │                   │                   │                │
      │<──────────────────│                   │                   │                │
```

---

## 4. 위험 및 대응

### 4.1 기술적 위험

| 위험 | 영향도 | 발생확률 | 대응 방안 |
|------|--------|----------|----------|
| 서버 인증서 미지원 | High | Low | 서버 관리자와 사전 협의 |
| 메모리 부족 | Medium | Medium | 버퍼 크기 최적화, 메모리 프로파일링 |
| TLS 핸드셰이크 타임아웃 | Medium | Medium | 타임아웃 값 조정, 재시도 로직 |
| 인증서 번들 크기 초과 | Low | Low | 필요한 CA만 포함하는 커스텀 번들 생성 |

### 4.2 운영적 위험

| 위험 | 영향도 | 발생확률 | 대응 방안 |
|------|--------|----------|----------|
| 인증서 만료 | High | Medium | 만료 알림 시스템, OTA 업데이트 준비 |
| 네트워크 방화벽 차단 | Medium | Low | 443 포트 허용 확인 문서화 |

---

## 5. 참고 자료

### 5.1 ESP-IDF 공식 문서

- [ESP HTTP Client](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32s3/api-reference/protocols/esp_http_client.html)
- [ESP-TLS](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32s3/api-reference/protocols/esp_tls.html)
- [Certificate Bundle](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32s3/api-reference/protocols/esp_crt_bundle.html)

### 5.2 예제 코드

- ESP-IDF 예제: `examples/protocols/https_request`
- ESP-IDF 예제: `examples/protocols/esp_http_client`

---

## 태그

`#SPEC-LICENSE-HTTPS-001` `#implementation-plan` `#https` `#security`
