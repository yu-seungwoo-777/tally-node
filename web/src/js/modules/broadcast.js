/**
 * 방송(LoRa) 모듈
 * 주파수 설정, 채널 스캔
 */

export function broadcastModule() {
    return {
        // Broadcast 주파수 프리셋 (EoRa-S3-900TB: 850-930MHz)
        channelPresets: {
            frequencies: [850.0, 860.0, 868.0, 870.0, 880.0, 900.0, 915.0, 925.0, 930.0]
        },

        // 채널 스캔 상태
        channelScan: {
            scanning: false,
            progress: 0,
            startFreq: 850.0,
            endFreq: 930.0,
            step: 1.0,
            results: [],
            recommendation: null,
            currentRssi: 0,
            currentStatus: 'unknown'
        },

        // 채널 스캔 폴링 타이머
        _channelScanPollingTimer: null,

        // 방송 채널 저장 상태
        broadcast: {
            saving: false
        },

        /**
         * 방송 채널 설정 저장
         */
        async saveBroadcast() {
            try {
                this.broadcast.saving = true;

                const res = await fetch('/api/config/device/rf', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({
                        frequency: Number(this.form.broadcast.frequency),
                        syncWord: Number(this.form.broadcast.syncCode)
                    })
                });
                const data = await res.json();
                if (data.status === 'ok') {
                    // 주파수 변경이 완료될 때까지 대기 (5-10초)
                    await new Promise(resolve => setTimeout(resolve, 6000));
                    await this.fetchStatus();
                    this.broadcast.saving = false;
                    this.showToast(`Channel saved: ${Math.round(this.form.broadcast.frequency)} MHz, Sync 0x${Number(this.form.broadcast.syncCode).toString(16).toUpperCase().padStart(2, '0')}`, 'alert-success');
                } else {
                    this.broadcast.saving = false;
                    this.showToast(data.message || 'Failed to save channel settings', 'alert-error');
                }
            } catch (e) {
                this.broadcast.saving = false;
                this.showToast('Network error. Please try again.', 'alert-error');
            }
        },

        /**
         * 채널 주파수 스캔 시작 (850-930 MHz, 1MHz step)
         */
        async startChannelScan() {
            try {
                this.channelScan.scanning = true;
                this.channelScan.progress = 0;
                this.channelScan.results = [];
                this.channelScan.recommendation = null;

                const res = await fetch('/api/lora/scan/start', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({
                        startFreq: 850.0,
                        endFreq: 930.0,
                        step: 1.0
                    })
                });
                const data = await res.json();
                if (data.status === 'started') {
                    this.showToast('Scanning 850-930 MHz...', 'alert-info');
                    this.startChannelScanPolling();
                } else {
                    this.channelScan.scanning = false;
                    this.showToast(data.message || 'Failed to start scan', 'alert-error');
                }
            } catch (e) {
                this.channelScan.scanning = false;
                this.showToast('Network error. Please try again.', 'alert-error');
            }
        },

        /**
         * 채널 주파수 스캔 중지
         */
        async stopChannelScan() {
            try {
                await fetch('/api/lora/scan/stop', { method: 'POST' });
                this.channelScan.scanning = false;
                this.stopChannelScanPolling();
                this.showToast('Scan cancelled', 'alert-warning');
            } catch (e) {
                this.showToast('Failed to stop scan', 'alert-error');
            }
        },

        /**
         * 채널 스캔 폴링 시작
         */
        startChannelScanPolling() {
            this.stopChannelScanPolling();
            this._channelScanPollingTimer = setInterval(async () => {
                await this.fetchChannelScanStatus();
            }, 500);
        },

        /**
         * 채널 스캔 폴링 중지
         */
        stopChannelScanPolling() {
            if (this._channelScanPollingTimer) {
                clearInterval(this._channelScanPollingTimer);
                this._channelScanPollingTimer = null;
            }
        },

        /**
         * 채널 스캔 상태 조회
         */
        async fetchChannelScanStatus() {
            try {
                const res = await fetch('/api/lora/scan');
                if (!res.ok) return;
                const data = await res.json();

                this.channelScan.scanning = data.scanning;
                this.channelScan.progress = data.progress || 0;

                if (data.results && data.results.length > 0) {
                    this.channelScan.results = data.results.map(r => {
                        // RSSI 퍼센트 계산: -100 dBm = 100%, -50 dBm = 0%
                        const gaugePercent = Math.max(0, Math.min(100, ((-50 - r.rssi) / 50) * 100));
                        return {
                            frequency: r.frequency,
                            rssi: r.rssi,
                            noiseFloor: r.noiseFloor,
                            clearChannel: r.clearChannel,
                            gaugePercent: gaugePercent.toFixed(1)
                        };
                    });

                    // 추천 분석
                    this.analyzeScanRecommendation();
                }

                if (!data.scanning && data.progress === 100) {
                    this.stopChannelScanPolling();
                    this.showToast(`Scan complete: ${data.results?.length || 0} channels found`, 'alert-success');
                }
            } catch (e) {
                console.error('Channel scan status fetch error:', e);
            }
        },

        /**
         * 스캔 결과 분석 및 추천
         */
        analyzeScanRecommendation() {
            if (!this.channelScan.results || this.channelScan.results.length === 0) {
                this.channelScan.recommendation = null;
                return;
            }

            const currentFreq = this.form.broadcast.frequency;
            const currentResult = this.channelScan.results.find(r => Math.abs(r.frequency - currentFreq) < 0.5);

            // 현재 주파수 RSSI/상태 업데이트
            if (currentResult) {
                this.channelScan.currentRssi = currentResult.rssi;
                this.channelScan.currentStatus = currentResult.clearChannel ? 'clear' : 'busy';
            } else {
                this.channelScan.currentRssi = 0;
                this.channelScan.currentStatus = 'unknown';
            }

            // 가장 조용한 채널 찾기
            const sortedByRssi = [...this.channelScan.results].sort((a, b) => a.rssi - b.rssi);
            const quietest = sortedByRssi[0];
            const quietestRssi = quietest.rssi;

            // 현재 채널 상태 확인
            if (currentResult) {
                if (currentResult.clearChannel) {
                    // 현재 채널이 조용함 - 추천 메시지 표시 안 함
                } else {
                    // 현재 채널이 노이즈함
                    this.channelScan.recommendation = {
                        type: 'change',
                        title: 'Current channel is noisy',
                        message: `Recommend switching to quieter channel`,
                        suggestedFreq: quietest.frequency
                    };
                }
            } else {
                // 현재 주파수가 스캔 범위 밖
                this.channelScan.recommendation = {
                    type: 'better',
                    title: 'Frequency outside scan range',
                    message: `Consider switching to ${quietest.frequency.toFixed(1)} MHz`,
                    suggestedFreq: quietest.frequency
                };
            }
        },

        /**
         * RSSI 게이지 색상 계산
         */
        getGaugeColor(rssi) {
            if (rssi <= -90) return '#22c55e';  // green-500
            if (rssi <= -80) return '#eab308';  // yellow-500
            return '#ef4444';  // red-500
        },

        /**
         * RSSI 게이지 stroke-dasharray 계산
         */
        getGaugeCircumference(rssi) {
            const r = 16;
            const circumference = 2 * Math.PI * r;  // 약 100.5
            // RSSI -100~ -50 범위를 0~100%로 변환
            const percent = Math.max(0, Math.min(100, ((-50 - rssi) / 50) * 100));
            const filled = (percent / 100) * circumference;
            return `${filled} ${circumference}`;
        },

        /**
         * RSSI 게이지 텍스트 색상
         */
        getGaugeTextColor(rssi) {
            if (rssi <= -90) return 'text-emerald-600';
            if (rssi <= -80) return 'text-yellow-600';
            return 'text-rose-600';
        },

        /**
         * 스캔 결과에서 주파수 선택
         */
        selectChannelFrequency(freq) {
            this.form.broadcast.frequency = freq;
            // 추천 메시지 유지 (갱신하지 않음)
            this.showToast(`Channel selected: ${Math.round(freq)} MHz - Click "Save Channel Settings" to apply`, 'alert-info');
        },

        /**
         * 추천 채널로 이동 (스크롤)
         */
        scrollToChannelSettings() {
            const el = document.getElementById('channel-settings');
            if (el) {
                el.scrollIntoView({ behavior: 'smooth' });
            }
        }
    };
}
