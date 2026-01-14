# SPEC-REFACTOR-002: Implementation Plan

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

---

## 1. Expert Analysis Summary

### 1.1 Recommended Design Approach: **Mobile-First Responsive Hybrid**

After analyzing the current implementation, the expert recommendation is a **hybrid approach** with mobile-first design principles:

**Rationale:**
1. **User Context**: Field technicians frequently use mobile devices for monitoring
2. **Technical Constraints**: Single-page application with Alpine.js requires careful state management
3. **Information Density**: Dashboard displays significant real-time data that needs reorganization for mobile

**Key Design Decisions:**
- Bottom navigation for mobile (< 768px) for better thumb reach
- Sidebar navigation for desktop (>= 768px) for efficient space use
- Responsive breakpoints at: 640px, 768px, 1024px
- Progressive disclosure for complex data on mobile

### 1.2 Current Implementation Issues

| Issue | Severity | Impact |
|-------|----------|--------|
| Sidebar takes full width on mobile | High | Poor mobile UX |
| Inconsistent card layouts | Medium | Visual confusion |
| Small touch targets on mobile | High | Accessibility issue |
| No mobile-specific navigation | High | Difficult mobile navigation |
- Fixed 264px sidebar on mobile
- Color inconsistency (7+ accent colors used)
- No swipe gestures for device cards
- Pull-to-refresh missing
- Modal dialogs not optimized for mobile

### 1.3 Recommended Component Structure Changes

**Current:**
```
index.html (main layout)
  +-- Sidebar (always visible on desktop, hidden mobile)
  +-- Header (page title)
  +-- Main (page content)
```

**Proposed:**
```
index.html (main layout)
  +-- Desktop Sidebar (>= 768px)
  +-- Mobile Header (< 768px, reduced)
  +-- Bottom Navigation (< 768px)
  +-- Main (page content)
      +-- Page-specific components
```

---

## 2. Implementation Milestones

### Milestone 1: Foundation (Primary Goal)

**Objective:** Establish responsive framework and navigation system

**Tasks:**
1. Configure Tailwind responsive breakpoints
2. Create bottom navigation component for mobile
3. Implement navigation state management
4. Update sidebar visibility logic
5. Add viewport meta tag refinements

**Deliverables:**
- Working mobile bottom navigation
- Proper sidebar/desktop navigation switching
- Navigation state preserved across views

**Acceptance:**
- Mobile shows 5-item bottom nav
- Desktop shows sidebar
- Switching works smoothly at 768px breakpoint

---

### Milestone 2: Page Refactoring - Dashboard & Devices (Primary Goal)

**Objective:** Optimize high-traffic pages for mobile

**Tasks:**
**Dashboard:**
1. Convert stats to horizontal scroll snap (mobile)
2. Make Switcher/Network cards collapsible
3. Redesign System Info as expandable section

**Devices:**
1. Redesign device card for mobile single-column
2. Implement swipe gestures (left: actions, right: delete)
3. Add FAB for global actions
4. Optimize status bar for mobile

**Deliverables:**
- Mobile-optimized Dashboard page
- Mobile-optimized Devices page with swipe actions

**Acceptance:**
- No horizontal scroll on mobile
- Device cards work with swipe gestures
- All controls accessible via touch

---

### Milestone 3: Page Refactoring - Network & Switcher (Secondary Goal)

**Objective:** Optimize configuration pages for mobile

**Tasks:**
**Network:**
1. Implement accordion pattern for settings sections
2. Combine status cards into compact view
3. Optimize form inputs for mobile

**Switcher:**
1. Make Dual Mode toggle sticky on mobile
2. Redesign camera mapping for horizontal scroll
3. Create compact PGM/PVW badges

**Deliverables:**
- Mobile-optimized Network page
- Mobile-optimized Switcher page

**Acceptance:**
- Forms work well on mobile
- Camera mapping usable on phone screens

---

### Milestone 4: Page Refactoring - Broadcast & License & System (Secondary Goal)

**Objective:** Complete remaining pages

**Tasks:**
**Broadcast:**
1. Redesign scan results as vertical list
2. Optimize frequency/sync sliders
3. Add fixed bottom scan button

**License:**
1. Optimize license input for mobile
2. Simplify test results display

**System:**
1. Redesign system action buttons
2. Optimize test mode controls
3. Improve notices list for mobile

**Deliverables:**
- Mobile-optimized Broadcast page
- Mobile-optimized License page
- Mobile-optimized System page

---

### Milestone 5: Design System Polish (Final Goal)

**Objective:** Unify design language across all pages

**Tasks:**
1. Standardize card component structure
2. Unify color usage (reduce to 5 semantic colors)
3. Create consistent spacing scale
4. Standardize typography sizes
5. Add loading states and transitions

**Deliverables:**
- Consistent visual design across all pages
- Design system documentation

---

### Milestone 6: Accessibility & Performance (Final Goal)

**Objective:** Ensure WCAG compliance and optimal performance

**Tasks:**
1. Audit all touch targets (minimum 44x44px)
2. Verify color contrast ratios
3. Add ARIA labels where missing
4. Implement focus management for modals
5. Optimize Alpine.js reactivity
6. Test on real devices

**Deliverables:**
- Accessibility audit report
- Performance benchmarks

---

## 3. Technical Approach

### 3.1 Responsive Breakpoint Strategy

```css
/* Tailwind configuration approach */
breakpoints: {
  'sm': '640px',   // Large phones
  'md': '768px',   // Tablets (nav switch)
  'lg': '1024px',  // Desktop
  'xl': '1280px',  // Large desktop
}
```

**Navigation Switch Logic:**
```javascript
// Alpine.js
navigationMode() {
  return window.innerWidth >= 768 ? 'sidebar' : 'bottom';
}
```

### 3.2 Component Architecture

**Shared Components:**
```
components/
├── Card/
│   ├── index.html
│   └── variants/ (stat-card, device-card, config-card)
├── Navigation/
│   ├── Sidebar.html
│   ├── BottomNav.html
│   └── MobileHeader.html
├── Forms/
│   ├── Input.html
│   ├── Slider.html
│   └── Dropdown.html
└── Feedback/
    ├── Toast.html
    └── Modal.html
```

### 3.3 State Management Strategy

**Alpine.js Store Pattern:**
```javascript
// stores/navigation.js
document.addEventListener('alpine:init', () => {
  Alpine.store('navigation', {
    currentView: 'dashboard',
    mobileNavOpen: false,
    bottomNavItems: ['dashboard', 'network', 'devices', 'more'],
    // ...
  });
});
```

### 3.4 Swipe Gesture Implementation

**Using Touch Events:**
```javascript
// Simple swipe detection
handleSwipe(element, actions) {
  let startX, startY;
  const threshold = 80; // px

  element.addEventListener('touchstart', (e) => {
    startX = e.touches[0].clientX;
    startY = e.touches[0].clientY;
  });

  element.addEventListener('touchend', (e) => {
    const endX = e.changedTouches[0].clientX;
    const diffX = startX - endX;
    const diffY = startY - e.changedTouches[0].clientY;

    if (Math.abs(diffX) > threshold && Math.abs(diffY) < 50) {
      if (diffX > 0) actions.onSwipeLeft();
      else actions.onSwipeRight();
    }
  });
}
```

---

## 4. File Modification Strategy

### 4.1 Files to Modify

| File | Type | Changes |
|------|------|---------|
| `index.html` | Layout | Add bottom nav, update sidebar logic |
| `pages/dashboard.html` | Page | Mobile card layouts |
| `pages/devices.html` | Page | Swipe gestures, card redesign |
| `pages/network.html` | Page | Accordion pattern |
| `pages/switcher.html` | Page | Mobile controls |
| `pages/broadcast.html` | Page | Vertical scan results |
| `pages/license.html` | Page | Mobile form |
| `pages/system.html` | Page | Mobile controls |
| `css/styles.css` | Styles | Add mobile-specific styles |

### 4.2 New Files to Create

| File | Purpose |
|------|---------|
| `css/mobile.css` | Mobile-specific styles |
| `js/components/bottomNav.js` | Bottom navigation component |
| `js/components/swipe.js` | Swipe gesture utility |

---

## 5. Risk Assessment

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| Alpine.js state complexity | Medium | Medium | Use Alpine.store for shared state |
| Breaking existing functionality | Low | High | Comprehensive testing after each milestone |
| Mobile browser compatibility | Low | Medium | Test on Safari (iOS) and Chrome (Android) |
| Performance degradation | Low | Medium | Profile rendering on low-end devices |

---

## 6. Dependencies

**Internal:**
- None (API contract unchanged)

**External:**
- Tailwind CSS CDN (existing)
- Alpine.js (existing)

---

## 7. Testing Strategy

### 7.1 Device Testing Matrix

| Device Type | Screen Size | Test Focus |
|-------------|-------------|------------|
| iPhone SE | 375x667 | Small phone layout |
| iPhone 14 Pro | 393x852 | Notch handling |
| Samsung Galaxy | 360x800 | Android compatibility |
| iPad Mini | 768x1024 | Tablet navigation |
| Desktop | 1920x1080+ | Sidebar layout |

### 7.2 Browser Testing

- Safari (iOS 16+)
- Chrome (Android 12+)
- Safari (macOS)
- Chrome (Windows/macOS)
- Firefox (secondary)

---

## 8. Rollback Strategy

**Git Branch Strategy:**
```bash
git checkout -b refactor/SPEC-REFACTOR-002-mobile-layout
# Work in milestones, commit after each
# Merge to master after full validation
```

**Rollback Plan:**
- Keep original `index.html` as `index.html.backup`
- Each milestone is a separate commit for easy revert
- Feature flags for gradual rollout (optional)

---

## 9. Success Criteria

### 9.1 Mobile UX Criteria

- No horizontal scroll on any page
- All touch targets >= 44x44px
- Navigation works with thumb-only operation
- Page transitions < 200ms

### 9.2 Desktop UX Criteria

- No regression in existing desktop experience
- Sidebar navigation works as before
- All features accessible

### 9.3 Quality Criteria

- Zero console errors
- WCAG AA color contrast
- 90%+ touch target compliance

---

## 10. Next Steps

After SPEC approval:

1. Create feature branch: `refactor/SPEC-REFACTOR-002-mobile-layout`
2. Execute Milestone 1 (Foundation)
3. User validation at Milestone 2
4. Continue with remaining milestones
5. Final QA before merge

**Estimated Completion:** 3-4 development cycles depending on feedback
