/**
 * Tally Test Mode Module
 * 테스트 모드 관련 기능
 */

export function testModule() {
    return {
        // 테스트 모드 상태
        testMode: {
            running: false,
            maxChannels: 4,
            interval: 500
        },

        /**
         * 테스트 모드 시작
         */
        async startTestMode() {
            try {
                // API 요청 전에 running 상태를 true로 설정하여 중복 클릭 방지
                this.testMode.running = true;

                const response = await fetch('/api/test/start', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({
                        max_channels: Number(this.testMode.maxChannels),
                        interval_ms: Number(this.testMode.interval)
                    })
                });

                if (response.ok) {
                    this.showToast('Test mode started', 'alert-info');
                } else {
                    // 응답 실패 시 running 상태 복원
                    this.testMode.running = false;
                    const error = await response.json();
                    this.showToast(error.message || 'Failed to start test mode', 'alert-error');
                }
            } catch (e) {
                // 에러 발생 시 running 상태 복원
                this.testMode.running = false;
                this.showToast('Failed to start test mode', 'alert-error');
            }
        },

        /**
         * 테스트 모드 중지
         */
        async stopTestMode() {
            try {
                const response = await fetch('/api/test/stop', {
                    method: 'POST'
                });

                if (response.ok) {
                    this.testMode.running = false;
                    this.showToast('Test mode stopped', 'alert-info');
                } else {
                    this.showToast('Failed to stop test mode', 'alert-error');
                }
            } catch (e) {
                this.showToast('Failed to stop test mode', 'alert-error');
            }
        }
    };
}
