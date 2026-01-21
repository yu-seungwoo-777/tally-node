/**
 * 유틸리티 모듈
 * 포맷팅, 토스트, 재부팅 등 공통 기능
 */

export function utilsModule() {
    return {
        // 다이얼로그 표시 상태
        showPrimaryConfig: false,
        showSecondaryConfig: false,
        showFactoryResetModal: false,
        factoryResetting: false,

        // 토스트 알림 상태
        toastState: {
            show: false,
            message: '',
            type: 'alert-info'
        },

        /**
         * 토스트 표시
         */
        showToast(msg, type = 'alert-info') {
            this.toastState.message = msg;
            this.toastState.type = type;
            this.toastState.show = true;
            setTimeout(() => {
                this.toastState.show = false;
            }, 3000);
        },

        /**
         * 토스트 표시 (별칭, state.js 등에서 사용)
         */
        toast(msg, type = 'alert-info') {
            this.toastState.message = msg;
            this.toastState.type = type;
            this.toastState.show = true;
            setTimeout(() => {
                this.toastState.show = false;
            }, 3000);
        },

        /**
         * 업타임 포맷 (HH:MM:SS)
         */
        formatUptimeShort(seconds) {
            const hours = Math.floor(seconds / 3600);
            const minutes = Math.floor((seconds % 3600) / 60);
            const secs = seconds % 60;
            const hh = String(hours).padStart(2, '0');
            const mm = String(minutes).padStart(2, '0');
            const ss = String(secs).padStart(2, '0');
            return `${hh}:${mm}:${ss}`;
        },

        /**
         * 업타임 포맷
         */
        formatUptime(seconds) {
            const hours = Math.floor(seconds / 3600);
            const minutes = Math.floor((seconds % 3600) / 60);
            if (hours > 0) {
                return `${hours}h ${minutes}m`;
            }
            return `${minutes}m`;
        },

        /**
         * RSSI와 SNR을 고려한 신호 레벨 (0-3)
         * icons.c의 getSignalLevel()과 동일한 로직
         */
        getSignalLevel(rssi, snr) {
            if (rssi > -70 && snr > 5) return 3;
            if (rssi > -85 && snr > 0) return 2;
            if (rssi > -100 && snr > -5) return 1;
            return 0;
        },

        /**
         * 신호 레벨 텍스트
         */
        getSignalText(rssi, snr) {
            const level = this.getSignalLevel(rssi, snr);
            if (level === 3) return 'Strong';
            if (level === 2) return 'Good';
            if (level === 1) return 'Weak';
            return 'No Signal';
        },

        /**
         * 바이트 포맷
         */
        formatBytes(bytes) {
            if (bytes < 1024) return bytes + ' B';
            if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB';
            return (bytes / (1024 * 1024)).toFixed(1) + ' MB';
        },

        /**
         * 페이지 전환
         */
        goTo(view) {
            this.currentView = view;
        },

        /**
         * 재부팅
         */
        async reboot() {
            if (!confirm('Are you sure you want to reboot the device?')) return;

            try {
                await fetch('/api/reboot', { method: 'POST' });
                this.showToast('Rebooting device...', 'alert-info');
            } catch (e) {
                this.showToast('Failed to reboot', 'alert-error');
            }
        },

        /**
         * 공장 초기화
         */
        async factoryReset() {
            this.factoryResetting = true;

            try {
                const response = await fetch('/api/factory-reset', { method: 'POST' });
                const data = await response.json();

                if (data.status === 'ok') {
                    this.showFactoryResetModal = false;
                    this.showToast('Factory reset in progress... System will reboot.', 'alert-info');

                    // 3초 후 리로드 (재부팅 후 연결 대기)
                    setTimeout(() => {
                        window.location.reload();
                    }, 3000);
                } else {
                    this.showToast('Factory reset failed: ' + (data.message || 'Unknown error'), 'alert-error');
                }
            } catch (e) {
                this.showToast('Factory reset failed: ' + e.message, 'alert-error');
            } finally {
                this.factoryResetting = false;
            }
        }
    };
}
