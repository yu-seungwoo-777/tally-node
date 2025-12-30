/**
 * Tally Node Web Application
 * Alpine.js 기반 메인 앱
 * 모듈화된 구조
 */

// 모듈 임포트
import { stateModule } from './modules/state.js';
import { networkModule } from './modules/network.js';
import { switcherModule } from './modules/switcher.js';
import { broadcastModule } from './modules/broadcast.js';
import { devicesModule } from './modules/devices.js';
import { deviceModule } from './modules/device.js';
import { utilsModule } from './modules/utils.js';

function tallyApp() {
    return {
        // 사이드바 상태
        sidebarOpen: false,

        // 각 모듈 병합
        ...stateModule(),
        ...networkModule(),
        ...switcherModule(),
        ...broadcastModule(),
        ...devicesModule(),
        ...deviceModule(),
        ...utilsModule()
    };
}

// Alpine.js 컴포넌트 등록
document.addEventListener('alpine:init', () => {
    Alpine.data('tallyApp', tallyApp);
});
