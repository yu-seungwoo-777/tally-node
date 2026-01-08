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

        // 밝기 제어 상태
        brightnessControl: {
            sending: null  // 전송 중인 디바이스 ID
        },

        // 카메라 ID 제어 상태
        cameraIdControl: {
            sending: null  // 전송 중인 디바이스 ID
        },

        // PING 제어 상태
        pingControl: {
            sending: null  // 전송 중인 디바이스 ID
        },

        // 상태 요청 제어 상태
        statusControl: {
            sending: false  // 전송 중 여부
        },

        // 기능 정지 제어 상태
        stopControl: {
            sending: null  // 전송 중인 디바이스 ID
        },

        // 재부팅 제어 상태
        rebootControl: {
            sending: null  // 전송 중인 디바이스 ID
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

                // 로컬 변경 중인 값 보존 (cameraIdControl, brightnessControl)
                const newDevices = data.devices || [];
                const oldDevices = this.devices.list || [];

                // 로컬 변경 값을 보존하면서 디바이스 목록 갱신
                this.devices.list = newDevices.map(newDevice => {
                    const oldDevice = oldDevices.find(d => d.id === newDevice.id);

                    // 로컬에서 변경 중인 cameraId가 있으면 보존
                    if (oldDevice && this.cameraIdControl[newDevice.id] !== undefined) {
                        newDevice.cameraId = this.cameraIdControl[newDevice.id];
                    }

                    // 로컬에서 변경 중인 brightness가 있으면 보존
                    if (oldDevice && this.brightnessControl[newDevice.id] !== undefined) {
                        newDevice.brightness = this.brightnessControl[newDevice.id];
                    }

                    return newDevice;
                });

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
         * 디바이스 밝기 설정
         * @param {string} deviceId - 디바이스 ID (예: "A1B2")
         * @param {number} brightness - 밝기 값 (0-100)
         */
        async setDeviceBrightness(deviceId, brightness) {
            // 밝기 정수 변환
            const brightnessValue = parseInt(brightness, 10);
            if (isNaN(brightnessValue) || brightnessValue < 0 || brightnessValue > 100) {
                console.error('Invalid brightness value:', brightness);
                return;
            }

            // 0-100 → 0-255 변환
            const brightness255 = Math.round((brightnessValue * 255) / 100);

            try {
                // 전송 중 표시
                this.brightnessControl.sending = deviceId;

                // 디바이스 ID 파싱 (hex 문자열 → 바이트 배열)
                const deviceIdBytes = [
                    parseInt(deviceId.substring(0, 2), 16),
                    parseInt(deviceId.substring(2, 4), 16)
                ];

                const res = await fetch('/api/device/brightness', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({
                        deviceId: deviceIdBytes,
                        brightness: brightness255
                    })
                });

                if (!res.ok) {
                    const errorData = await res.json().catch(() => ({}));
                    throw new Error(errorData.message || 'Failed to set brightness');
                }

                const data = await res.json();
                if (data.status === 'ok') {
                    console.log(`Brightness set for ${deviceId}: ${brightnessValue}% (${brightness255})`);
                    // 로컬 devices.list에도 즉시 반영 (폴링과 충돌 방지)
                    const device = this.devices.list.find(d => d.id === deviceId);
                    if (device) {
                        device.brightness = brightnessValue;
                    }
                    // 임시 입력값 삭제 (이제 devices.list 값 사용)
                    delete this.brightnessControl[deviceId];
                } else {
                    throw new Error(data.message || 'Failed to set brightness');
                }
            } catch (e) {
                console.error('Set brightness error:', e);
                // 사용자 피드백 (간단한 alert 또는 toast)
                alert(`밝기 설정 실패: ${e.message}`);
            } finally {
                // 전송 중 표시 해제
                this.brightnessControl.sending = null;
            }
        },

        /**
         * 디바이스 카메라 ID 설정
         * @param {string} deviceId - 디바이스 ID (예: "A1B2")
         * @param {number} cameraId - 카메라 ID (1-20)
         */
        async setDeviceCameraId(deviceId, cameraId) {
            const cameraIdValue = parseInt(cameraId, 10);
            if (isNaN(cameraIdValue) || cameraIdValue < 1 || cameraIdValue > 20) {
                console.error('Invalid camera ID:', cameraId);
                return;
            }

            try {
                // 전송 중 표시
                this.cameraIdControl.sending = deviceId;

                // 디바이스 ID 파싱 (hex 문자열 → 바이트 배열)
                const deviceIdBytes = [
                    parseInt(deviceId.substring(0, 2), 16),
                    parseInt(deviceId.substring(2, 4), 16)
                ];

                const res = await fetch('/api/device/camera-id', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({
                        deviceId: deviceIdBytes,
                        cameraId: cameraIdValue
                    })
                });

                if (!res.ok) {
                    const errorData = await res.json().catch(() => ({}));
                    throw new Error(errorData.message || 'Failed to set camera ID');
                }

                const data = await res.json();
                if (data.status === 'ok') {
                    console.log(`Camera ID set for ${deviceId}: ${cameraIdValue}`);
                    // 로컬 devices.list에도 즉시 반영 (폴링과 충돌 방지)
                    const device = this.devices.list.find(d => d.id === deviceId);
                    if (device) {
                        device.cameraId = cameraIdValue;
                    }
                    // 임시 입력값 삭제 (이제 devices.list 값 사용)
                    delete this.cameraIdControl[deviceId];
                } else {
                    throw new Error(data.message || 'Failed to set camera ID');
                }
            } catch (e) {
                console.error('Set camera ID error:', e);
                alert(`카메라 ID 설정 실패: ${e.message}`);
            } finally {
                // 전송 중 표시 해제
                this.cameraIdControl.sending = null;
            }
        },

        /**
         * 카메라 ID 조정 (+/- 버튼)
         * @param {string} deviceId - 디바이스 ID
         * @param {number} delta - 변경량 (-1 또는 +1)
         */
        adjustCameraId(deviceId, delta) {
            const currentId = this.cameraIdControl[deviceId] ?? this.devices.list.find(d => d.id === deviceId)?.cameraId ?? 1;
            const newId = currentId + delta;
            if (newId >= 1 && newId <= 20) {
                this.cameraIdControl[deviceId] = newId;
                this.setDeviceCameraId(deviceId, newId);
            }
        },

        /**
         * 디바이스 PING 전송
         * @param {string} deviceId - 디바이스 ID (예: "A1B2")
         */
        async pingDevice(deviceId) {
            try {
                // 전송 중 표시
                this.pingControl.sending = deviceId;

                // 디바이스 ID 파싱 (hex 문자열 → 바이트 배열)
                const deviceIdBytes = [
                    parseInt(deviceId.substring(0, 2), 16),
                    parseInt(deviceId.substring(2, 4), 16)
                ];

                const res = await fetch('/api/device/ping', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({
                        deviceId: deviceIdBytes
                    })
                });

                if (!res.ok) {
                    const errorData = await res.json().catch(() => ({}));
                    throw new Error(errorData.message || 'Failed to send ping');
                }

                const data = await res.json();
                if (data.status === 'ok') {
                    console.log(`PING sent to ${deviceId}`);
                    // PONG 응답은 디바이스 목록 폴링으로 업데이트됨
                } else {
                    throw new Error(data.message || 'Failed to send ping');
                }
            } catch (e) {
                console.error('PING error:', e);
                // 사용자 피드백
                alert(`PING 실패: ${e.message}`);
            } finally {
                // 전송 중 표시 해제
                this.pingControl.sending = null;
            }
        },

        /**
         * 상태 요청 전송 (Broadcast)
         * 모든 RX 디바이스에 상태 요청 전송
         */
        async requestStatus() {
            try {
                // 전송 중 표시
                this.statusControl.sending = true;

                const res = await fetch('/api/device/status-request', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' }
                });

                if (!res.ok) {
                    throw new Error('Failed to send status request');
                }

                const data = await res.json();
                if (data.status === 'ok') {
                    console.log('Status request sent');
                    // 상태 응답은 디바이스 목록 폴링으로 업데이트됨
                } else {
                    throw new Error(data.message || 'Failed to send status request');
                }
            } catch (e) {
                console.error('Status request error:', e);
                alert(`상태 요청 실패: ${e.message}`);
            } finally {
                // 전송 중 표시 해제
                this.statusControl.sending = false;
            }
        },

        /**
         * 디바이스 기능 정지/재개
         * @param {string} deviceId - 디바이스 ID (예: "A1B2")
         */
        async stopDevice(deviceId) {
            try {
                // 전송 중 표시
                this.stopControl.sending = deviceId;

                // 디바이스 ID 파싱 (hex 문자열 → 바이트 배열)
                const deviceIdBytes = [
                    parseInt(deviceId.substring(0, 2), 16),
                    parseInt(deviceId.substring(2, 4), 16)
                ];

                const res = await fetch('/api/device/stop', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({
                        deviceId: deviceIdBytes
                    })
                });

                if (!res.ok) {
                    throw new Error('Failed to send stop command');
                }

                const data = await res.json();
                if (data.status === 'ok') {
                    console.log(`Stop command sent to ${deviceId}`);
                    // 상태 업데이트를 위해 디바이스 목록 새로고침
                    setTimeout(() => this.fetchDevices(), 500);
                } else {
                    throw new Error(data.message || 'Failed to send stop command');
                }
            } catch (e) {
                console.error('Stop command error:', e);
                alert(`기능 정지 실패: ${e.message}`);
            } finally {
                // 전송 중 표시 해제
                this.stopControl.sending = null;
            }
        },

        /**
         * 디바이스 재부팅
         * @param {string} deviceId - 디바이스 ID (예: "A1B2")
         */
        async rebootDevice(deviceId) {
            try {
                // 확인 대화상자
                if (!confirm(`${deviceId} 디바이스를 재부팅하시겠습니까?`)) {
                    return;
                }

                // 전송 중 표시
                this.rebootControl.sending = deviceId;

                // 디바이스 ID 파싱 (hex 문자열 → 바이트 배열)
                const deviceIdBytes = [
                    parseInt(deviceId.substring(0, 2), 16),
                    parseInt(deviceId.substring(2, 4), 16)
                ];

                const res = await fetch('/api/device/reboot', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({
                        deviceId: deviceIdBytes
                    })
                });

                if (!res.ok) {
                    throw new Error('Failed to send reboot command');
                }

                const data = await res.json();
                if (data.status === 'ok') {
                    console.log(`Reboot command sent to ${deviceId}`);
                    alert(`${deviceId} 디바이스에 재부팅 명령을 전송했습니다.`);
                } else {
                    throw new Error(data.message || 'Failed to send reboot command');
                }
            } catch (e) {
                console.error('Reboot command error:', e);
                if (e.message !== 'User canceled') {
                    alert(`재부팅 실패: ${e.message}`);
                }
            } finally {
                // 전송 중 표시 해제
                this.rebootControl.sending = null;
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
