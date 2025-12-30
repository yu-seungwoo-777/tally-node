/**
 * 디바이스 모듈
 * 온라인 RX 디바이스 목록 조회
 */

export function devicesModule() {
    return {
        // 디바이스 데이터
        devices: {
            list: [],
            onlineCount: 0,
            registeredCount: 0,
            loading: false,
            _pollingTimer: null
        },

        /**
         * 초기화
         */
        async initDevices() {
            // 처음 로드
            await this.fetchDevices();

            // currentView가 devices면 폴링 시작
            this.$watch('currentView', (value) => {
                if (value === 'devices') {
                    this.startDevicesPolling();
                } else {
                    this.stopDevicesPolling();
                }
            });

            // 초기 로드 시 devices 페이지면 폴링 시작
            if (this.currentView === 'devices') {
                this.startDevicesPolling();
            }
        },

        /**
         * 디바이스 목록 조회
         */
        async fetchDevices() {
            try {
                this.devices.loading = true;
                const res = await fetch('/api/devices');
                if (!res.ok) throw new Error('Failed to fetch devices');

                const data = await res.json();

                this.devices.list = data.devices || [];
                this.devices.onlineCount = data.count || 0;
                this.devices.registeredCount = data.registeredCount || 0;
            } catch (e) {
                console.error('Devices fetch error:', e);
                // 에러 시 빈 배열 표시
                this.devices.list = [];
                this.devices.onlineCount = 0;
                this.devices.registeredCount = 0;
            } finally {
                this.devices.loading = false;
            }
        },

        /**
         * 디바이스 폴링 시작
         */
        startDevicesPolling() {
            if (this.devices._pollingTimer) return;

            // 5초마다 조회
            this.devices._pollingTimer = setInterval(async () => {
                await this.fetchDevices();
            }, 5000);
        },

        /**
         * 디바이스 폴링 중지
         */
        stopDevicesPolling() {
            if (this.devices._pollingTimer) {
                clearInterval(this.devices._pollingTimer);
                this.devices._pollingTimer = null;
            }
        },

        /**
         * 업타임 포맷 (초 → 읽기 쉬운 형식)
         */
        formatUptime(seconds) {
            if (!seconds) return '0s';

            const hours = Math.floor(seconds / 3600);
            const minutes = Math.floor((seconds % 3600) / 60);
            const secs = seconds % 60;

            if (hours > 0) {
                return `${hours}h ${minutes}m`;
            } else if (minutes > 0) {
                return `${minutes}m ${secs}s`;
            } else {
                return `${secs}s`;
            }
        }
    };
}
