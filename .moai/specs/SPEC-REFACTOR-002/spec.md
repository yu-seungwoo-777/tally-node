# SPEC-REFACTOR-002: Web Design Layout Refactoring

## TAG BLOCK

```
TAG: SPEC-REFACTOR-002
TYPE: REFACTOR
DOMAIN: FRONTEND_UI_UX
PRIORITY: HIGH
STATUS: PLANNED
CREATED: 2025-01-15
AUTHOR: 유승우
```

## 1. Environment

### 1.1 Project Context

- **Project Name**: Tally Node Control
- **Purpose**: Wireless tally system web interface
- **Target Users**: Broadcast engineers, camera operators, technical directors
- **Device Context**: Both desktop (control room) and mobile (field monitoring)

### 1.2 Technology Stack

| Component | Technology | Version |
|-----------|------------|---------|
| Styling | Tailwind CSS | CDN (latest) |
| JavaScript Framework | Alpine.js | 3.x |
| Architecture | Single Page Application | - |
| Build Tool | Custom build script | - |

### 1.3 Current State

**File Structure:**
```
web/src/
├── index.html          # Main layout (sidebar, header, main)
├── pages/
│   ├── dashboard.html  # Status overview
│   ├── network.html    # AP/WiFi/Ethernet settings
│   ├── switcher.html   # Video switcher configuration
│   ├── broadcast.html  # RF channel settings
│   ├── devices.html    # Connected device management
│   ├── license.html    # License activation
│   └── system.html     # System settings
├── css/
│   └── styles.css      # Custom styles
└── js/
    └── app.js          # Alpine.js entry point
```

**Known Constraints:**
- API MUST NOT be changed (backend contract fixed)
- WebSocket communication must remain intact
- Alpine.js state management must be preserved
- Build process uses custom placeholder replacement

---

## 2. Assumptions

### 2.1 User Behavior Assumptions

| Assumption | Confidence | Impact if Wrong |
|------------|------------|-----------------|
| Users access dashboard primarily from desktop in control room | High | Mobile experience secondary priority |
| Field technicians use mobile for device monitoring | Medium | Mobile UX needs optimization |
| Users are familiar with technical interfaces | High | Can use dense information displays |
| Tablets are used for portable monitoring | Medium | Need tablet-specific layouts |

### 2.2 Technical Assumptions

| Assumption | Confidence | Impact if Wrong |
|------------|------------|-----------------|
| Tailwind CDN provides sufficient styling capability | High | May need build step for advanced features |
| Alpine.js x-data pattern supports component restructure | High | May need module refactoring |
| Browser support: Chrome, Safari, Firefox (modern) | High | May drop legacy browser support |

---

## 3. Requirements (EARS Format)

### 3.1 Ubiquitous Requirements (Always Active)

**REQ-001:** 시스템은 모든 페이지에서 일관된 디자인 시스템(색상, 간격, 타이포그래피)을 적용해야 한다.

**REQ-002:** 시스템은 모든 반응형 브레이크포인트에서 접근성을 보장해야 한다.

**REQ-003:** 시스템은 API 변경 없이 UI/UX만 개선해야 한다.

### 3.2 Event-Driven Requirements

**REQ-004:** WHEN 사용자가 모바일 화면에서 진입 시 THEN 시스템은 하단 네비게이션을 표시해야 한다.

**REQ-005:** WHEN 사용자가 데스크톱 화면에서 진입 시 THEN 시스템은 사이드바 네비게이션을 표시해야 한다.

**REQ-006:** WHEN 사용자가 화면 크기를 변경(리사이즈) 시 THEN 시스템은 768px 브레이크포인트에서 네비게이션 스타일을 전환해야 한다.

**REQ-007:** WHEN 사용자가 디바이스 카드를 스와이프 시 THEN 시스템은 빠른 액션 메뉴를 표시해야 한다.

### 3.3 State-Driven Requirements

**REQ-008:** IF 네트워크 연결이 오프라인 상태이면 THEN 시스템은 오프라인 배너를 표시해야 한다.

**REQ-009:** IF 라이선스가 유효하지 않으면 THEN 시스템은 경고 배너를 모든 페이지 상단에 표시해야 한다.

**REQ-010:** IF 디바이스 목록이 10개 이상이면 THEN 시스템은 모바일에서 축약된 카드 뷰를 제공해야 한다.

### 3.4 Optional Requirements

**REQ-011:** 가능하면 시스템은 다크 모드 테마를 지원해야 한다.

**REQ-012:** 가능하면 시스템은 사용자의 네비게이션 위치를 기억해야 한다.

### 3.5 Unwanted Behavior Requirements

**REQ-013:** 시스템은 모바일에서 가로 스크롤을 생성해서는 안 된다.

**REQ-014:** 시스템은 터치 타겟을 44x44px 미만으로 제공해서는 안 된다.

**REQ-015:** 시스템은 리사이즈 중에 레이아웃이 깨지는 현상을 방지해야 한다.

---

## 4. Specifications

### 4.1 Responsive Breakpoint Strategy

| Breakpoint | Width | Target Device | Navigation |
|------------|-------|---------------|------------|
| Mobile | < 640px | Phones | Bottom nav (5 items) |
| Mobile Large | 640px - 767px | Large phones | Bottom nav (5 items) |
| Tablet | 768px - 1023px | Tablets | Sidebar (collapsed) |
| Desktop | >= 1024px | Desktops | Sidebar (expanded) |

### 4.2 Navigation Architecture

#### Desktop (>= 1024px)
```
+----------+------------------------+
| Sidebar  |  Header                |
|          |  (Page Title)          |
|          +------------------------+
|  - Dash  |                        |
|  - Net   |  Main Content          |
|  - Swt   |                        |
|  - Brd   |                        |
|  - Dev   |                        |
|  - Lic   |                        |
|  - Sys   |                        |
+----------+------------------------+
```

#### Mobile (< 768px)
```
+------------------------+
|  Header (Status bar)    |
+------------------------+
|                        |
|  Main Content          |
|                        |
|                        |
+------------------------+
|  Bottom Navigation     |
|  [Dash][Net][Swt][+][..]|
+------------------------+
```

### 4.3 Component Design Specifications

#### 4.3.1 Card Component Standard

```html
<!-- Standard Card Structure -->
<div class="card">
  <div class="card-header">
    <div class="card-icon">...</div>
    <div class="card-title-group">
      <h3 class="card-title">Title</h3>
      <p class="card-subtitle">Subtitle</p>
    </div>
    <div class="card-actions">...</div>
  </div>
  <div class="card-body">...</div>
  <div class="card-footer">...</div>
</div>
```

**Responsive Behavior:**
- Desktop: Full padding, horizontal layout
- Mobile: Reduced padding, vertical stacking

#### 4.3.2 Device Card (Mobile Optimized)

```
Mobile Layout:
+-------------------+
| (Icon) CAM-01  [>]|
| Online | 85%      |
| [Battery]  Ping   |
| Brightness [slider]|
+-------------------+
```

**Swipe Actions:**
- Left swipe: Quick settings
- Right swipe: Delete/Reboot

#### 4.3.3 Form Component Standard

**Input Fields:**
- Minimum touch target: 44px height
- Spacing: 16px between fields
- Labels: Above input (mobile), inline options (desktop)

### 4.4 Color System

| Semantic | Hex | Usage |
|----------|-----|-------|
| Primary | #3b82f6 | Primary actions, links |
| Success | #10b981 | Connected, online status |
| Warning | #f59e0b | Intermediate states |
| Error | #ef4444 | Disconnected, offline |
| Neutral | #64748b | Secondary text |
| Background | #f8fafc | Page background |

### 4.5 Typography Scale

| Size | Mobile | Desktop | Usage |
|------|--------|---------|-------|
| xs | 12px | 12px | Labels, meta |
| sm | 14px | 14px | Body, secondary |
| base | 16px | 16px | Default text |
| lg | 18px | 18px | Card titles |
| xl | 20px | 24px | Page headers |
| 2xl | 24px | 30px | Hero titles |

---

## 5. Page-by-Page Specifications

### 5.1 Dashboard Page

**Mobile Optimizations:**
- Stats: Horizontal scroll snap container
- Switcher/Network cards: Collapsible sections
- System Info: Hidden behind "Show More" button

**Desktop Layout:**
- Stats: 3-column grid
- Switcher + Network: 2-column grid
- System Info: Full-width footer card

### 5.2 Network Page

**Mobile Optimizations:**
- AP/WiFi/Ethernet: Stack vertically
- Status cards: Combine into compact view
- Settings: Accordion pattern (one section open at a time)

**Desktop Layout:**
- Settings + Status pairs: 2-column grid
- All network sections visible

### 5.3 Switcher Page

**Mobile Optimizations:**
- Dual mode toggle: Top sticky header
- Camera mapping: Horizontal scroll grid
- PGM/PVW status: Compact badges

**Desktop Layout:**
- Switcher cards: 2-column grid
- Camera mapping: 10-column grid (full width)

### 5.4 Broadcast Page

**Mobile Optimizations:**
- Scan results: Vertical list with large touch targets
- Channel settings: Single column, large sliders
- Scan button: Fixed bottom position

**Desktop Layout:**
- 2-column grid (Scan + Results left, Settings right)

### 5.5 Devices Page

**Mobile Optimizations:**
- Device cards: Single column
- Status bar: Compact horizontal
- Quick actions: Swipe gestures + FAB (Floating Action Button)

**Desktop Layout:**
- Device cards: 3-column grid
- Status bar: Full width

### 5.6 License Page

**Mobile Optimizations:**
- Cards: Stack vertically
- License input: Large text field with automatic focus
- Test button: Full width

**Desktop Layout:**
- 2-column grid

### 5.7 System Page

**Mobile Optimizations:**
- System actions: Large danger zone buttons
- Test mode: Full-width controls
- Notices: Compact list with expandable details
- Update speed: Large touch-friendly buttons

**Desktop Layout:**
- 2-column grid (Test Mode + Update Speed top, Notices bottom)

---

## 6. Mobile Interaction Patterns

### 6.1 Bottom Navigation Specification

**Items (5 primary):**
1. Dashboard (home icon)
2. Network (wifi icon)
3. Devices (devices icon)
4. More (grid icon) - opens overlay with: Switcher, Broadcast, License, System

**Behavior:**
- Fixed position, bottom: 0
- Height: 64px
- Active state: Background color + icon fill
- Safe area support for notched devices

### 6.2 Swipe Gesture Specification

**Device Cards:**
- Left swipe (20% threshold): Reveal quick actions
  - Stop/Resume button
  - Brightness preset buttons
- Right swipe (20% threshold): Reveal destructive actions
  - Delete button (red)
  - Reboot button (orange)

### 6.3 Pull-to-Refresh

**Pages with live data:**
- Dashboard
- Devices
- Network (status cards)

**Implementation:**
- Standard pull gesture (60px threshold)
- Loading indicator during refresh
- Success feedback

---

## 7. Accessibility Requirements

### 7.1 Touch Targets

| Element Type | Minimum Size | Recommended |
|--------------|--------------|-------------|
| Buttons | 44x44px | 48x48px |
| Links | 44x44px height | - |
| Form inputs | 44px height | 48px |
| Checkboxes | 44x44px | - |

### 7.2 Color Contrast

- Normal text: WCAG AA (4.5:1)
- Large text: WCAG AA (3:1)
- UI components: WCAG AA (3:1)

### 7.3 Screen Reader Support

- Navigation: ARIA labels
- Status updates: aria-live regions
- Modals: Focus trap + aria-modal

---

## 8. Performance Requirements

**REQ-PERF-001:** 시스템은 첫 번째 콘텐츠 페인트(FCP)를 1.5초 이내에 달성해야 한다.

**REQ-PERF-002:** 시스템은 상호 작용 다음 페인트(INP)를 100ms 이내로 처리해야 한다.

**REQ-PERF-003:** 시스템은 누적 레이아웃 변경(CLS)을 0.1 미만으로 유지해야 한다.

---

## 9. Traceability

| Requirement | Component | Test Scenario |
|-------------|-----------|---------------|
| REQ-004 | Bottom Navigation | TC-NAV-001 |
| REQ-005 | Sidebar | TC-NAV-002 |
| REQ-007 | Device Card Swipe | TC-DEV-001 |
| REQ-013 | Viewport Meta | TC-ACC-001 |
| REQ-014 | Touch Targets | TC-ACC-002 |

---

## 10. Appendix

### 10.1 Design Principles

1. **Content First**: Information hierarchy takes precedence
2. **Progressive Disclosure**: Show essential info first, details on demand
3. **Touch Optimized**: Design for fingers, not cursors
4. **Consistent Patterns**: Reusable components across pages

### 10.2 Terminology

| Term | Definition |
|------|------------|
| FAB | Floating Action Button |
| PGM | Program (live) video output |
| PVW | Preview video output |
| RX | Receiver device |
| TX | Transmitter device |
