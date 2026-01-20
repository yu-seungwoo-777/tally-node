# SPEC-LICENSE-HTTPS-001: 인수 기준

## 관련 SPEC

- **SPEC ID**: SPEC-LICENSE-HTTPS-001
- **제목**: 라이센스 인증 시스템 HTTPS 지원

---

## 1. 인수 기준 개요

### 1.1 완료 정의 (Definition of Done)

- [ ] 모든 기능 요구사항 (FR-01 ~ FR-04) 구현 완료
- [ ] 모든 비기능 요구사항 (NFR-01 ~ NFR-03) 충족
- [ ] 모든 테스트 시나리오 통과
- [ ] 코드 리뷰 완료
- [ ] 문서 업데이트 완료

### 1.2 품질 게이트

| 항목 | 기준 | 현재 상태 |
|------|------|----------|
| 빌드 성공 | 컴파일 오류 0개 | - |
| 경고 | 컴파일 경고 0개 | - |
| 기능 테스트 | 100% 통과 | - |
| 메모리 사용량 | 힙 증가 20KB 이하 | - |
| 응답 시간 | 평균 20초 이하 | - |

---

## 2. 테스트 시나리오

### 2.1 TC-01: 정상 HTTPS 라이센스 검증

**Scenario**: 유효한 라이센스 키로 HTTPS 검증 성공

```gherkin
Given 디바이스가 WiFi에 연결되어 있고
  And 유효한 라이센스 키가 설정되어 있고
  And 라이센스 서버가 HTTPS로 접근 가능할 때
When license_client_validate() 함수를 호출하면
Then 응답의 success 필드가 true이고
  And device_limit 값이 반환되고
  And 통신이 TLS로 암호화되어야 한다
```

**검증 방법**:
- 네트워크 패킷 캡처로 TLS 핸드셰이크 확인
- Wireshark로 암호화된 페이로드 확인

---

### 2.2 TC-02: 인증서 검증 성공

**Scenario**: 유효한 서버 인증서로 연결 수립

```gherkin
Given 라이센스 서버가 유효한 CA 인증서를 사용하고
  And ESP32의 인증서 번들이 해당 CA를 포함할 때
When HTTPS 연결을 시도하면
Then TLS 핸드셰이크가 성공하고
  And 인증서 검증이 통과되고
  And 연결이 수립되어야 한다
```

**검증 방법**:
- 로그에서 SSL 핸드셰이크 성공 메시지 확인
- `esp_http_client_perform()` 반환값 ESP_OK 확인

---

### 2.3 TC-03: 잘못된 인증서 거부

**Scenario**: 자체 서명 인증서 거부

```gherkin
Given 테스트 서버가 자체 서명 인증서를 사용할 때
When HTTPS 연결을 시도하면
Then 인증서 검증이 실패하고
  And LICENSE_ERR_CERT_INVALID 오류가 반환되고
  And 연결이 거부되어야 한다
```

**검증 방법**:
- 자체 서명 인증서 서버로 테스트
- 오류 코드 및 로그 메시지 확인

---

### 2.4 TC-04: 만료된 인증서 거부

**Scenario**: 만료된 서버 인증서 거부

```gherkin
Given 테스트 서버의 인증서가 만료되었을 때
When HTTPS 연결을 시도하면
Then 인증서 만료 검증이 실패하고
  And LICENSE_ERR_CERT_EXPIRED 오류가 반환되고
  And 적절한 오류 메시지가 로깅되어야 한다
```

**검증 방법**:
- 만료된 인증서 서버로 테스트
- 오류 메시지에 "expired" 또는 "만료" 포함 확인

---

### 2.5 TC-05: SSL 핸드셰이크 실패 처리

**Scenario**: TLS 버전 불일치로 핸드셰이크 실패

```gherkin
Given 서버가 TLS 1.0만 지원하고
  And 클라이언트가 TLS 1.2 이상만 지원할 때
When HTTPS 연결을 시도하면
Then SSL 핸드셰이크가 실패하고
  And LICENSE_ERR_SSL_HANDSHAKE 오류가 반환되고
  And 오류가 적절히 로깅되어야 한다
```

**검증 방법**:
- TLS 1.0 전용 테스트 서버 사용
- 핸드셰이크 실패 로그 확인

---

### 2.6 TC-06: 네트워크 타임아웃 처리

**Scenario**: 서버 응답 지연으로 타임아웃

```gherkin
Given 라이센스 서버가 응답하지 않을 때
When HTTPS 연결을 시도하고
  And LICENSE_HTTPS_TIMEOUT_MS (20초)가 경과하면
Then 연결이 타임아웃되고
  And LICENSE_ERR_CONNECTION_FAILED 오류가 반환되고
  And 리소스가 정상적으로 해제되어야 한다
```

**검증 방법**:
- 서버 방화벽으로 응답 차단
- 타임아웃 시간 측정 (20초 ± 1초)
- 메모리 누수 확인

---

### 2.7 TC-07: HTTP 폴백 방지

**Scenario**: HTTPS 실패 시 HTTP로 폴백하지 않음

```gherkin
Given HTTPS 연결이 실패할 때
When 재시도 또는 폴백 로직이 실행되면
Then HTTP (포트 80)로 연결을 시도하지 않고
  And 오류가 상위 레이어로 전파되어야 한다
```

**검증 방법**:
- 코드에서 "http://" 문자열 검색 (제거 확인)
- 네트워크 캡처로 포트 80 트래픽 없음 확인

---

### 2.8 TC-08: 연결 테스트 HTTPS 전환

**Scenario**: connection_test 함수 HTTPS 사용

```gherkin
Given license_client_connection_test() 함수가 호출될 때
When 서버 연결 테스트를 수행하면
Then HTTPS를 통해 /api/connection-test 엔드포인트에 접근하고
  And TLS 암호화가 적용되어야 한다
```

**검증 방법**:
- 함수 호출 후 네트워크 패킷 확인
- 로그에서 HTTPS URL 확인

---

### 2.9 TC-09: 메모리 사용량 검증

**Scenario**: HTTPS 전환 후 메모리 사용량 허용 범위 내

```gherkin
Given HTTPS 라이센스 검증이 구현되었을 때
When 라이센스 검증을 10회 연속 수행하면
Then 힙 사용량 증가가 20KB 이하이고
  And 메모리 누수가 발생하지 않아야 한다
```

**검증 방법**:
- `esp_get_free_heap_size()` 호출하여 측정
- 검증 전후 힙 크기 비교
- 10회 반복 후 힙 크기 안정화 확인

---

### 2.10 TC-10: 성능 검증

**Scenario**: HTTPS 응답 시간 허용 범위 내

```gherkin
Given HTTPS 라이센스 검증이 구현되었을 때
When 라이센스 검증을 5회 수행하면
Then 평균 응답 시간이 20초 이하이고
  And 최대 응답 시간이 30초 이하여야 한다
```

**검증 방법**:
- 각 요청의 시작/종료 시간 측정
- 평균 및 최대값 계산
- P95 응답 시간 기록

---

## 3. 보안 검증

### 3.1 SV-01: 데이터 암호화 검증

**목표**: 전송 데이터가 암호화되었는지 확인

**검증 항목**:
- [ ] 라이센스 키가 암호화된 채널로 전송됨
- [ ] MAC 주소가 암호화된 채널로 전송됨
- [ ] API 키가 암호화된 채널로 전송됨

**검증 방법**:
1. Wireshark로 네트워크 트래픽 캡처
2. TLS 레코드 프로토콜 확인
3. 평문 데이터 노출 없음 확인

---

### 3.2 SV-02: 인증서 체인 검증

**목표**: 인증서 체인이 올바르게 검증되는지 확인

**검증 항목**:
- [ ] 루트 CA 검증
- [ ] 중간 인증서 검증
- [ ] 서버 인증서 검증
- [ ] CN (Common Name) 일치 확인

---

### 3.3 SV-03: 프로토콜 버전 검증

**목표**: 안전한 TLS 버전만 사용

**검증 항목**:
- [ ] TLS 1.2 이상 사용
- [ ] SSL 3.0, TLS 1.0, TLS 1.1 비활성화

---

## 4. 회귀 테스트

### 4.1 RT-01: 기존 기능 호환성

**Scenario**: 기존 라이센스 검증 로직 정상 동작

```gherkin
Given HTTPS가 구현된 license_client가 있을 때
When 기존 license_service.cpp에서 호출하면
Then API 인터페이스가 동일하게 유지되고
  And 반환값 형식이 변경되지 않아야 한다
```

---

### 4.2 RT-02: 오류 응답 호환성

**Scenario**: 서버 오류 응답 처리

```gherkin
Given 라이센스 서버가 오류 응답을 반환할 때
When JSON 오류 응답을 파싱하면
Then 기존과 동일한 형식으로 error 메시지가 추출되어야 한다
```

---

## 5. 검증 체크리스트

### 5.1 구현 완료 체크리스트

- [ ] `license_client.h` - URL HTTPS로 변경
- [ ] `license_client.h` - 타임아웃 상수 추가
- [ ] `license_client.h` - 오류 열거형 추가
- [ ] `license_client.cpp` - SSL 설정 추가
- [ ] `license_client.cpp` - 인증서 번들 연결
- [ ] `license_client.cpp` - 오류 처리 개선
- [ ] `CMakeLists.txt` - 의존성 추가
- [ ] `sdkconfig` - mbedTLS 설정

### 5.2 테스트 완료 체크리스트

- [ ] TC-01: 정상 HTTPS 검증
- [ ] TC-02: 인증서 검증 성공
- [ ] TC-03: 잘못된 인증서 거부
- [ ] TC-04: 만료된 인증서 거부
- [ ] TC-05: SSL 핸드셰이크 실패
- [ ] TC-06: 네트워크 타임아웃
- [ ] TC-07: HTTP 폴백 방지
- [ ] TC-08: connection_test HTTPS
- [ ] TC-09: 메모리 사용량
- [ ] TC-10: 성능 검증

### 5.3 보안 검증 체크리스트

- [ ] SV-01: 데이터 암호화
- [ ] SV-02: 인증서 체인
- [ ] SV-03: 프로토콜 버전

### 5.4 회귀 테스트 체크리스트

- [ ] RT-01: 기존 기능 호환성
- [ ] RT-02: 오류 응답 호환성

---

## 6. 승인

| 역할 | 담당자 | 승인일 | 서명 |
|------|--------|--------|------|
| 개발자 | - | - | - |
| 리뷰어 | - | - | - |
| QA | - | - | - |

---

## 태그

`#SPEC-LICENSE-HTTPS-001` `#acceptance-criteria` `#testing` `#security`
