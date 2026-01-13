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

        // LED 색상 상태
        ledColors: {
            program: { r: 255, g: 0, b: 0 },
            preview: { r: 0, g: 255, b: 0 },
            off: { r: 0, g: 0, b: 0 },
            saving: false
        },

        /**
         * 테스트 모드 시작
         */
        async startTestMode() {
            try {
                const response = await fetch('/api/test/start', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({
                        max_channels: Number(this.testMode.maxChannels),
                        interval_ms: Number(this.testMode.interval)
                    })
                });

                if (response.ok) {
                    this.testMode.running = true;
                    this.showToast('Test mode started', 'alert-info');
                } else {
                    const error = await response.json();
                    this.showToast(error.message || 'Failed to start test mode', 'alert-error');
                }
            } catch (e) {
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
        },

        /**
         * LED 색상 저장
         */
        async saveLedColors() {
            this.ledColors.saving = true;
            try {
                const response = await fetch('/api/led/colors', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({
                        program: {
                            r: Number(this.ledColors.program.r),
                            g: Number(this.ledColors.program.g),
                            b: Number(this.ledColors.program.b)
                        },
                        preview: {
                            r: Number(this.ledColors.preview.r),
                            g: Number(this.ledColors.preview.g),
                            b: Number(this.ledColors.preview.b)
                        },
                        off: {
                            r: Number(this.ledColors.off.r),
                            g: Number(this.ledColors.off.g),
                            b: Number(this.ledColors.off.b)
                        }
                    })
                });

                if (response.ok) {
                    this.showToast('LED colors saved', 'alert-success');
                } else {
                    this.showToast('Failed to save LED colors', 'alert-error');
                }
            } catch (e) {
                this.showToast('Failed to save LED colors', 'alert-error');
            } finally {
                this.ledColors.saving = false;
            }
        },

        /**
         * LED 색상 로드
         */
        async loadLedColors() {
            try {
                const response = await fetch('/api/led/colors');
                if (response.ok) {
                    const data = await response.json();
                    this.ledColors.program = data.program;
                    this.ledColors.preview = data.preview;
                    this.ledColors.off = data.off;
                }
            } catch (e) {
                console.error('Failed to load LED colors', e);
            }
        },

        /**
         * RGB를 Hex로 변환 (color input용)
         */
        rgbToHex(r, g, b) {
            return '#' + [r, g, b].map(x => {
                const hex = Number(x).toString(16);
                return hex.length === 1 ? '0' + hex : hex;
            }).join('');
        },

        /**
         * Color input 변경 시 RGB 업데이트
         */
        updateLedColor(type, hex) {
            const r = parseInt(hex.slice(1, 3), 16);
            const g = parseInt(hex.slice(3, 5), 16);
            const b = parseInt(hex.slice(5, 7), 16);
            this.ledColors[type].r = r;
            this.ledColors[type].g = g;
            this.ledColors[type].b = b;
        }
    };
}
