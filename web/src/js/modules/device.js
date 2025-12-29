/**
 * 장치 모듈
 * 디스플레이, 카메라 ID 설정
 */

export function deviceModule() {
    return {
        /**
         * 디스플레이 밝기 저장
         */
        async saveBrightness() {
            try {
                const res = await fetch('/api/config/device/brightness', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ value: this.form.display.brightness })
                });
                const data = await res.json();
                if (data.status === 'ok') {
                    this.showToast(`Brightness saved: ${this.form.display.brightness}`, 'alert-success');
                } else {
                    this.showToast(data.message || 'Failed to save brightness', 'alert-error');
                }
            } catch (e) {
                this.showToast('Network error. Please try again.', 'alert-error');
            }
        },

        /**
         * 카메라 ID 저장
         */
        async saveCameraId() {
            try {
                const res = await fetch('/api/config/device/camera_id', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ value: this.form.display.cameraId })
                });
                const data = await res.json();
                if (data.status === 'ok') {
                    this.showToast(`Camera ID saved: ${this.form.display.cameraId}`, 'alert-success');
                } else {
                    this.showToast(data.message || 'Failed to save camera ID', 'alert-error');
                }
            } catch (e) {
                this.showToast('Network error. Please try again.', 'alert-error');
            }
        }
    };
}
