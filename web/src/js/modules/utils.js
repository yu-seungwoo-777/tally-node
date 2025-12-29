/**
 * 유틸리티 모듈
 * 포맷팅, 토스트, 재부팅 등 공통 기능
 */

export function utilsModule() {
    return {
        // 다이얼로그 표시 상태
        showPrimaryConfig: false,
        showSecondaryConfig: false,

        // 토스트 알림
        toast: {
            show: false,
            message: '',
            type: 'alert-info'
        },

        /**
         * 토스트 표시
         */
        showToast(msg, type = 'alert-info') {
            this.toast.message = msg;
            this.toast.type = type;
            this.toast.show = true;
            setTimeout(() => {
                this.toast.show = false;
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
        }
    };
}
