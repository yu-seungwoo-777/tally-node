# SPEC-WEB-001: 인수 기준

## 개요

이 문서는 SPEC-WEB-001의 구현 완료 여부를 검증하기 위한 인수 기준을 정의합니다.

---

## 정의 (Definition)

| 용어 | 정의 |
|------|------|
| **결합 Tally** | Primary와 Secondary 스위처의 Tally 정보를 합쳐서 표시한 데이터 |
| **PGM** | Program 상태 (방송 중인 채널) |
| **PVW** | Preview 상태 (다음 순서 채널) |
| **듀얼 모드** | Primary와 Secondary 스위처를 동시에 사용하는 모드 |

---

## Given-When-Then 테스트 시나리오

### Scenario 1: 듀얼 모드 활성화 시 결합 Tally 표시

**Given**:
- 듀얼 모드가 활성화되어 있음 (`form.switcher.dualEnabled == true`)
- Primary 스위처가 연결되어 있고 PGM에 채널 1, PVW에 채널 2가 있음
- Secondary 스위처가 연결되어 있고 PGM에 채널 5, PVW에 채널 6이 있음
- API가 `combined: { pgm: [1, 5], pvw: [2, 6] }`을 반환함

**When**:
- 사용자가 스위처 설정 페이지에 접속함

**Then**:
- "Combined Tally (S1 + S2)" 카드가 표시됨
- PGM 박스에 "1, 5"가 빨간색으로 표시됨
- PVW 박스에 "2, 6"이 초록색으로 표시됨

---

### Scenario 2: 듀얼 모드 비활성화 시 결합 Tally 숨김

**Given**:
- 듀얼 모드가 비활성화되어 있음 (`form.switcher.dualEnabled == false`)
- 이전에 결합 Tally 데이터가 있었을 수 있음

**When**:
- 사용자가 스위처 설정 페이지에 접속함

**Then**:
- "Combined Tally" 카드가 표시되지 않음
- 페이지에 다른 요소들이 정상적으로 렌더링됨

---

### Scenario 3: 결합 데이터 없음 처리

**Given**:
- 듀얼 모드가 활성화되어 있음
- API가 `combined: null`을 반환하거나 `combined` 필드가 없음

**When**:
- 사용자가 스위처 설정 페이지에 접속함

**Then**:
- "Combined Tally" 카드가 표시되지 않음 (x-show 조건 미충족)
- JavaScript 콘솔에 오류가 없음

---

### Scenario 4: PGM/PVW 빈 배열 처리

**Given**:
- 듀얼 모드가 활성화되어 있음
- API가 `combined: { pgm: [], pvw: [], channels: 0 }`을 반환함

**When**:
- 사용자가 스위처 설정 페이지에 접속함

**Then**:
- "Combined Tally" 카드가 표시됨
- PGM 박스에 "-"가 표시됨
- PVW 박스에 "-"가 표시됨

---

### Scenario 5: 실시간 업데이트

**Given**:
- 듀얼 모드가 활성화되어 있음
- 스위처 설정 페이지가 열려 있음
- 초기 결합 Tally: PGM [1], PVW [2]

**When**:
- Primary 스위처에서 Tally 상태가 변경되어 PGM이 [3]로 바뀜
- 폴링 간격 후 API가 새로운 데이터를 반환함

**Then**:
- "Combined Tally" 카드의 PGM 박스가 "3"으로 자동 업데이트됨
- 페이지 새로고침 없이 Alpine.js 반응성으로 업데이트됨

---

### Scenario 6: 듀얼 모드 토글 시 동적 표시/숨김

**Given**:
- 사용자가 스위처 설정 페이지에 있음
- 듀얼 모드가 비활성화되어 있음

**When**:
- 사용자가 듀얼 모드 토글을 ON으로 변경함
- API 요청이 성공하고 `combined` 데이터가 수신됨

**Then**:
- "Combined Tally" 카드가 페이드인 애니메이션과 함께 표시됨
- Primary/Secondary 카드 아래, Camera Mapping 위에 위치함

**When (part 2)**:
- 사용자가 듀얼 모드 토글을 OFF로 변경함

**Then (part 2)**:
- "Combined Tally" 카드가 숨겨짐

---

## 품질 게이트 (Quality Gates)

### QG-1: 기능 완결성
- [ ] 듀얼 모드 ON 시 결합 Tally 카드 표시
- [ ] 듀얼 모드 OFF 시 결합 Tally 카드 숨김
- [ ] PGM/PVW 배열이 올바르게 표시됨
- [ ] 빈 배열 시 "-" 표시

### QG-2: 데이터 무결성
- [ ] API `combined` 데이터가 정확히 파싱됨
- [ ] Primary/Secondary 개별 Tally와 합계가 일치함
- [ ] 채널 번호 중복 없이 올바르게 표시됨

### QG-3: UI/UX 품질
- [ ] 기존 Primary/Secondary 카드와 일관된 스타일
- [ ] 반응형 레이아웃 (모바일/데스크톱)
- [ ] 부드러운 전환 애니메이션
- [ ] 색상 대비 충분 (접근성)

### QG-4: 기술 안정성
- [ ] JavaScript 오류 없음
- [ ] Alpine.js 반응성 정상 작동
- [ ] null/undefined 안전 처리
- [ ] 메모리 누수 없음

---

## 검증 방법

### 수동 테스트 절차

1. **환경 설정**:
   - TX 모드 Tally Node를 네트워크에 연결
   - 두 개의 스위처 (ATEM, vMix)를 연결하거나 시뮬레이션

2. **듀얼 모드 활성화**:
   - 웹 UI 접속 → Switcher 페이지
   - Dual Mode 토글 ON
   - Secondary 스위처 설정

3. **결합 Tally 확인**:
   - Primary/Secondary 카드 아래에 "Combined Tally" 카드 확인
   - PGM/PVW 채널 번호가 올바른지 확인

4. **실시간 업데이트 확인**:
   - 스위처에서 Tally 상태 변경
   - 웹 UI에서 폴링 간격 후 업데이트 확인

5. **듀얼 모드 비활성화**:
   - Dual Mode 토글 OFF
   - 결합 Tally 카드가 사라지는지 확인

### 자동화 가능 테스트 (향후)

```javascript
// 테스트 가이드 (수동 검증용)
const verifyCombinedTally = () => {
    // 1. 듀얼 모드 확인
    const dualEnabled = window.Alpine.store('state').form.switcher.dualEnabled;
    console.assert(dualEnabled, 'Dual mode should be enabled');

    // 2. 결합 데이터 확인
    const combined = window.Alpine.store('state').status.switcher.combined;
    console.assert(combined !== null, 'Combined data should exist');
    console.assert(Array.isArray(combined.pgm), 'PGM should be array');
    console.assert(Array.isArray(combined.pvw), 'PVW should be array');

    // 3. UI 요소 확인
    const combinedCard = document.querySelector('[x-show*="combined"]');
    console.assert(combinedCard !== null, 'Combined card should be in DOM');

    console.log('All checks passed!');
};
```

---

## Definition of Done

다음 조건이 모두 충족될 때 SPEC-WEB-001이 완료된 것으로 간주합니다:

1. **구현 완료**:
   - [ ] `state.js`에 `combined` 데이터 파싱 로직 구현
   - [ ] `switcher.html`에 결합 Tally 카드 추가
   - [ ] 듀얼 모드 조건부 표시 구현

2. **테스트 통과**:
   - [ ] 모든 Given-When-Then 시나리오 통과
   - [ ] 모든 품질 게이트 항목 충족

3. **코드 품질**:
   - [ ] 기존 코드 스타일과 일관성 유지
   - [ ] 주석 추가 (필요시)
   - [ ] 불필요한 코드 제거

4. **문서화**:
   - [ ] SPEC 문서 완료
   - [ ] 구현 변경사항 기록
