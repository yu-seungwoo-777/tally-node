/**
 * 상태 모듈
 * 상태 초기화, fetchStatus, 폴링
 */

export function stateModule() {
    return {
        // 초기화 완료 플래그
        _initialized: false,

        // 폴링 타이머
        _statusPollingTimer: null,

        // 현재 뷰
        currentView: 'dashboard',

        // 뷰 타이틀 매핑
        viewTitles: {
            dashboard: 'Dashboard',
            network: 'Network',
            switcher: 'Switcher',
            broadcast: 'Broadcast',
            devices: 'Devices',
            system: 'System',
            license: 'License Management'
        },

        // WebSocket 연결 상태
        wsConnected: false,

        // 밝기 조절 모달 상태
        showBrightnessModal: false,

        // LED 색상 모달 상태
        showLedColorsModal: false,

        // /api/status 응답 데이터 (캐시)
        status: {
            network: { wifi: { connected: false, ssid: '', ip: '--' }, ethernet: { connected: false, ip: '--' } },
            switcher: {
                dualEnabled: false,
                secondaryOffset: 4,
                primary: { connected: false, type: 'ATEM', ip: '', port: 0, tally: { pgm: [], pvw: [], raw: '', channels: 0 } },
                secondary: { connected: false, type: 'ATEM', ip: '', port: 0, tally: { pgm: [], pvw: [], raw: '', channels: 0 } },
                combined: { pgm: [], pvw: [], raw: '', channels: 0 }
            },
            system: { deviceId: '0000', battery: 0, voltage: 0, temperature: 0, uptime: 0 }
        },

        // 네트워크 상태 (UI용 별칭)
        network: {
            wifiConnected: false,
            wifiSsid: '',
            wifiIp: '--',
            ethEnabled: false,
            ethConnected: false,
            ethDetected: false,
            ethIp: '--',
            apEnabled: false,
            apSsid: '',
            apChannel: 1,
            apIp: '--'
        },

        // 시스템 정보
        system: {
            deviceId: '0000',
            battery: 0,
            voltage: 0,
            temperature: 0,
            uptime: 0,
            freeHeap: 0,
            version: '2.0.1',
            loraChipType: 1  // 1=SX1262_868M, 2=SX1268_433M
        },

        // 설정 데이터
        config: {
            network: {
                wifiAp: { ssid: '', channel: 1, enabled: false },
                wifiSta: { ssid: '', enabled: false },
                ethernet: { dhcp: true, staticIp: '', netmask: '', gateway: '', enabled: false }
            },
            switcher: {
                primary: { connected: false, type: 'ATEM', ip: '', port: 0, interface: 2, cameraLimit: 0, tally: { pgm: [], pvw: [], raw: '', channels: 0 } },
                secondary: { connected: false, type: 'ATEM', ip: '', port: 0, interface: 1, cameraLimit: 0, tally: { pgm: [], pvw: [], raw: '', channels: 0 } },
                dualEnabled: false,
                secondaryOffset: 4
            },
            broadcast: {
                rf: {
                    frequency: 868,
                    syncWord: 18,
                    spreadingFactor: 7,
                    codingRate: 7,
                    bandwidth: 250,
                    txPower: 22
                }
            },
            // UI 폴링 간격 (ms)
            pollingInterval: 2000
        },

        // LED 색상 (API status에서 가져옴)
        ledColors: {
            program: { r: 255, g: 0, b: 0 },
            preview: { r: 0, g: 255, b: 0 },
            off: { r: 0, g: 0, b: 0 },
            saving: false
        },

        // 폼 입력 임시 데이터
        form: {
            ap: { ssid: '', password: '', channel: 1, enabled: false },
            wifi: { ssid: '', password: '', enabled: false },
            ethernet: { dhcp: true, ip: '', gateway: '', netmask: '', enabled: false },
            switcher: {
                primary: { type: 'ATEM', ip: '', port: 9910, interface: 0, cameraLimit: 0, password: '', portLocked: true },
                secondary: { type: 'ATEM', ip: '', port: 9910, interface: 0, cameraLimit: 0, password: '', portLocked: true },
                dualEnabled: false,
                secondaryOffset: 4
            },
            mappingOffset: 4,
            display: { brightness: 128, cameraId: 1 },
            broadcast: { syncCode: 18, frequency: 868.0 }
        },

        /**
         * 뷰 타이틀 계산
         */
        viewTitle() {
            return this.viewTitles[this.currentView] || 'TALLY-NODE';
        },

        /**
         * 초기화
         */
        async init() {
            console.log('Tally App initializing...');

            // URL 해시에서 현재 뷰 복원
            const hash = window.location.hash.slice(1);
            if (hash && ['dashboard', 'network', 'switcher', 'broadcast', 'devices', 'system', 'license'].includes(hash)) {
                this.currentView = hash;
            }

            // 해시 변경 감지
            window.addEventListener('hashchange', () => {
                const newHash = window.location.hash.slice(1);
                if (newHash && ['dashboard', 'network', 'switcher', 'broadcast', 'devices', 'system', 'license'].includes(newHash)) {
                    this.currentView = newHash;
                }
            });

            await this.fetchStatus();

            // 브로드캐스트 모듈 초기화 (칩 타입별 주파수 설정)
            // DOM 업데이트를 위해 nextTick 사용
            this.$nextTick(() => {
                if (this.initBroadcastModule) {
                    this.initBroadcastModule();
                }
            });

            // 디바이스 모듈 초기화
            await this.initDevices();

            // 라이센스 모듈 초기화
            await this.initLicense();

            // 상태 폴링 시작 (모든 페이지)
            this.startStatusPolling();

            // currentView 감시 (system 페이지 진입 시 공지사항 로드)
            this.$watch('currentView', (value) => {
                if (value === 'system' && this.notices.list.length === 0) {
                    this.fetchNotices();
                }
            });

            // 초기 로드 시 system 페이지면 공지사항 로드
            if (this.currentView === 'system') {
                this.fetchNotices();
            }
        },

        /**
         * 상태 조회
         */
        async fetchStatus() {
            try {
                const res = await fetch('/api/status');
                if (!res.ok) throw new Error('Status fetch failed');
                const data = await res.json();

                // status 캐시 업데이트
                this.status = { ...this.status, ...data };

                // Network (상태 + 설정 통합 업데이트)
                if (data.network) {
                    // AP
                    if (data.network.ap) {
                        this.network.apEnabled = data.network.ap.enabled || false;
                        this.network.apSsid = data.network.ap.ssid || '';
                        this.network.apChannel = data.network.ap.channel || 1;
                        this.network.apIp = data.network.ap.ip || '--';
                        // 첫 로드 시에만 폼 초기화
                        if (!this._initialized) {
                            this.form.ap = {
                                enabled: data.network.ap.enabled,
                                ssid: data.network.ap.ssid || '',
                                channel: data.network.ap.channel || 1,
                                password: data.network.ap.password || ''
                            };
                        }
                    }
                    // WiFi STA
                    if (data.network.wifi) {
                        this.network.wifiConnected = data.network.wifi.connected;
                        this.network.wifiSsid = data.network.wifi.ssid || '';
                        this.network.wifiIp = data.network.wifi.ip || '--';
                        // 첫 로드 시에만 폼 초기화
                        if (!this._initialized) {
                            this.form.wifi.enabled = data.network.wifi.enabled;
                            this.form.wifi.ssid = data.network.wifi.ssid || '';
                            this.form.wifi.password = data.network.wifi.password || '';
                        }
                    }
                    // Ethernet
                    if (data.network.ethernet) {
                        this.network.ethEnabled = data.network.ethernet.enabled || false;
                        this.network.ethConnected = data.network.ethernet.connected;
                        this.network.ethDetected = data.network.ethernet.detected || false;
                        this.network.ethIp = data.network.ethernet.ip || '--';
                        // 첫 로드 시에만 폼 초기화
                        if (!this._initialized) {
                            this.form.ethernet.enabled = data.network.ethernet.enabled;
                            this.form.ethernet.dhcp = data.network.ethernet.dhcp;
                            this.form.ethernet.ip = data.network.ethernet.staticIp || '';
                            this.form.ethernet.netmask = data.network.ethernet.netmask || '';
                            this.form.ethernet.gateway = data.network.ethernet.gateway || '';
                        }
                    }
                }

                // System (UI용 업데이트)
                if (data.system) {
                    this.system.deviceId = data.system.deviceId || '0000';
                    this.system.battery = data.system.battery || 0;
                    this.system.voltage = data.system.voltage || 0;
                    this.system.temperature = data.system.temperature || 0;
                    this.system.uptime = data.system.uptime || 0;
                    this.system.loraChipType = data.system.loraChipType || 1;
                    // 펌웨어 버전 (API에서 가져옴)
                    this.system.version = data.system.version || '2.0.1';
                }

                // Switcher 업데이트 (primary/secondary에 상태+설정 병합)
                if (data.switcher) {
                    // config는 항상 업데이트 (카드 표시용)
                    this.config.switcher.dualEnabled = data.switcher.dualEnabled || false;
                    this.config.switcher.secondaryOffset = data.switcher.secondaryOffset || 4;

                    // 폼은 초기화 시에만 업데이트 (폴링 시 폼 입력 방지 방지)
                    if (!this._initialized) {
                        this.form.switcher.dualEnabled = data.switcher.dualEnabled || false;
                        this.form.switcher.secondaryOffset = data.switcher.secondaryOffset || 4;
                        this.form.mappingOffset = data.switcher.secondaryOffset || 4; // 1-based 그대로 사용
                    }

                    if (data.switcher.primary) {
                        // config 업데이트
                        const primaryData = {
                            connected: data.switcher.primary.connected || false,
                            type: data.switcher.primary.type || 'ATEM',
                            ip: data.switcher.primary.ip || '',
                            port: data.switcher.primary.port || 0,
                            interface: data.switcher.primary.interface || 0,
                            cameraLimit: data.switcher.primary.cameraLimit || 0,
                            tally: data.switcher.primary.tally || { pgm: [], pvw: [], raw: '', channels: 0 }
                        };
                        this.config.switcher.primary = primaryData;

                        // status에도 업데이트 (새 객체 생성 - Alpine.js 반응성)
                        if (!this.status.switcher) this.status.switcher = { primary: {}, secondary: {} };
                        this.status.switcher.primary = { ...primaryData };

                        // 첫 로드 시 폼 초기화
                        if (!this._initialized) {
                            this.form.switcher.primary.type = data.switcher.primary.type || 'ATEM';
                            this.form.switcher.primary.ip = data.switcher.primary.ip || '';
                            this.form.switcher.primary.port = data.switcher.primary.port || 9910;
                            this.form.switcher.primary.interface = data.switcher.primary.interface || 0;
                            this.form.switcher.primary.cameraLimit = data.switcher.primary.cameraLimit || 0;
                            this.form.switcher.primary.password = data.switcher.primary.password || '';
                            this.form.switcher.primary.portLocked = true;
                        }
                    }
                    if (data.switcher.secondary) {
                        // config 업데이트
                        const secondaryData = {
                            connected: data.switcher.secondary.connected || false,
                            type: data.switcher.secondary.type || 'ATEM',
                            ip: data.switcher.secondary.ip || '',
                            port: data.switcher.secondary.port || 0,
                            interface: data.switcher.secondary.interface || 0,
                            cameraLimit: data.switcher.secondary.cameraLimit || 0,
                            tally: data.switcher.secondary.tally || { pgm: [], pvw: [], raw: '', channels: 0 }
                        };
                        this.config.switcher.secondary = secondaryData;

                        // status에도 업데이트 (새 객체 생성 - Alpine.js 반응성)
                        if (!this.status.switcher) this.status.switcher = { primary: {}, secondary: {} };
                        this.status.switcher.secondary = { ...secondaryData };

                        // 첫 로드 시 폼 초기화
                        if (!this._initialized) {
                            this.form.switcher.secondary.type = data.switcher.secondary.type || 'ATEM';
                            this.form.switcher.secondary.ip = data.switcher.secondary.ip || '';
                            this.form.switcher.secondary.port = data.switcher.secondary.port || 9910;
                            this.form.switcher.secondary.interface = data.switcher.secondary.interface || 0;
                            this.form.switcher.secondary.cameraLimit = data.switcher.secondary.cameraLimit || 0;
                            this.form.switcher.secondary.password = data.switcher.secondary.password || '';
                            this.form.switcher.secondary.portLocked = true;
                        }
                    }

                    // Combined Tally 데이터 업데이트 (듀얼 모드 결합 PGM/PVW)
                    if (data.switcher.combined) {
                        const combinedData = {
                            pgm: data.switcher.combined.pgm || [],
                            pvw: data.switcher.combined.pvw || [],
                            raw: data.switcher.combined.raw || '',
                            channels: data.switcher.combined.channels || 0
                        };
                        // status에 업데이트 (새 객체 생성 - Alpine.js 반응성)
                        if (!this.status.switcher) this.status.switcher = {};
                        this.status.switcher.combined = { ...combinedData };
                    } else {
                        // combined 데이터가 없으면 초기값 유지
                        if (!this.status.switcher) this.status.switcher = {};
                        if (!this.status.switcher.combined) {
                            this.status.switcher.combined = { pgm: [], pvw: [], raw: '', channels: 0 };
                        }
                    }
                }

                // Broadcast 설정 업데이트 (RF 설정만 포함)
                if (data.broadcast && data.broadcast.rf) {
                    this.config.broadcast.rf = {
                        frequency: data.broadcast.rf.frequency || 868,
                        syncWord: data.broadcast.rf.syncWord || 18,
                        spreadingFactor: data.broadcast.rf.spreadingFactor || 7,
                        codingRate: data.broadcast.rf.codingRate || 7,
                        bandwidth: data.broadcast.rf.bandwidth || 250,
                        txPower: data.broadcast.rf.txPower || 22
                    };
                    // 첫 로드 시 폼 초기화
                    if (!this._initialized) {
                        this.form.broadcast.syncCode = data.broadcast.rf.syncWord || 18;
                        this.form.broadcast.frequency = data.broadcast.rf.frequency || 868.0;
                    }
                } else {
                    // broadcast.rf가 없는 경우 기본값 설정
                    this.config.broadcast.rf = {
                        frequency: 868,
                        syncWord: 18,
                        spreadingFactor: 7,
                        codingRate: 7,
                        bandwidth: 250,
                        txPower: 22
                    };
                    if (!this._initialized) {
                        this.form.broadcast.syncCode = 18;
                        this.form.broadcast.frequency = 868.0;
                    }
                }

                // LED 색상 업데이트 (첫 로드 시에만 - 사용자 변경 방지)
                if (data.led && !this._initialized) {
                    if (data.led.program) {
                        this.ledColors.program = {
                            r: data.led.program.r || 255,
                            g: data.led.program.g || 0,
                            b: data.led.program.b || 0
                        };
                    }
                    if (data.led.preview) {
                        this.ledColors.preview = {
                            r: data.led.preview.r || 0,
                            g: data.led.preview.g || 255,
                            b: data.led.preview.b || 0
                        };
                    }
                    if (data.led.off) {
                        this.ledColors.off = {
                            r: data.led.off.r || 0,
                            g: data.led.off.g || 0,
                            b: data.led.off.b || 0
                        };
                    }
                }

                // 초기화 완료 표시
                this._initialized = true;

                this.wsConnected = true;
            } catch (e) {
                console.error('Status fetch error:', e);
                this.wsConnected = false;
            }
        },

        /**
         * 상태 폴링 시작 (스위처 카드 정보 주기 업데이트)
         */
        startStatusPolling() {
            // 이미 실행 중이면 중지 후 재시작
            this.stopStatusPolling();

            // localStorage에서 설정 로드
            const saved = localStorage.getItem('pollingInterval');
            if (saved) {
                this.config.pollingInterval = parseInt(saved);
            }

            // 설정된 간격으로 상태 조회
            this._statusPollingTimer = setInterval(async () => {
                await this.fetchStatus();
            }, this.config.pollingInterval);
        },

        /**
         * 상태 폴링 중지
         */
        stopStatusPolling() {
            if (this._statusPollingTimer) {
                clearInterval(this._statusPollingTimer);
                this._statusPollingTimer = null;
            }
        },

        /**
         * 폴링 속도 설정 (ms)
         * @param {number} interval - 폴링 간격 (500, 1000, 2000, 5000)
         */
        setPollingInterval(interval) {
            const validIntervals = [500, 1000, 2000, 5000];
            if (!validIntervals.includes(interval)) {
                interval = 2000; // 기본값
            }
            this.config.pollingInterval = interval;
            localStorage.setItem('pollingInterval', interval.toString());

            // 폴링 중이면 재시작
            if (this._statusPollingTimer) {
                this.startStatusPolling();
            }
        },

        // ========================================================================
        // System Settings
        // ========================================================================

        /**
         * 디스플레이 밝기 저장
         */
        async saveBrightness() {
            try {
                const res = await fetch('/api/display/brightness', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ brightness: this.form.display.brightness })
                });
                if (!res.ok) throw new Error('Failed to save brightness');
                const data = await res.json();
                if (data.status === 'ok') {
                    this.toast('Brightness saved', 'alert-success');
                } else {
                    throw new Error(data.message || 'Save failed');
                }
            } catch (e) {
                console.error('Save brightness error:', e);
                this.toast(e.message, 'alert-error');
            }
        },

        /**
         * TX 재부팅 (이 기기만)
         */
        async rebootTx() {
            if (!confirm('Are you sure you want to reboot this device?')) {
                return;
            }
            try {
                const res = await fetch('/api/reboot', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' }
                });
                if (!res.ok) throw new Error('Failed to reboot');
                this.toast('Device is rebooting...', 'alert-info');
            } catch (e) {
                console.error('Reboot error:', e);
                this.toast(e.message, 'alert-error');
            }
        },

        /**
         * 전체 재부팅 (브로드캐스트)
         */
        async rebootAll() {
            if (!confirm('Are you sure you want to reboot ALL devices?')) {
                return;
            }
            try {
                const res = await fetch('/api/reboot/broadcast', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' }
                });
                if (!res.ok) throw new Error('Failed to broadcast reboot');
                this.toast('Reboot command sent to all devices', 'alert-info');
            } catch (e) {
                console.error('Broadcast reboot error:', e);
                this.toast(e.message, 'alert-error');
            }
        },

        // ========================================================================
        // Global Brightness Control (TX → all RX)
        // ========================================================================

        // 일괄 밝기 제어 상태
        globalBrightness: {
            sending: false,
            value: 50  // 0-100%
        },

        // 공지사항 상태
        notices: {
            list: [],
            loading: false,
            error: null
        },

        // 공지사항 모달 상태
        noticeModal: {
            show: false,
            notice: null
        },

        /**
         * 전체 RX 디바이스 밝기 일괄 설정 (Broadcast)
         * @param {number} brightness - 밝기 값 (0-100)
         */
        async setGlobalBrightness(brightness) {
            const brightnessValue = parseInt(brightness, 10);
            if (isNaN(brightnessValue) || brightnessValue < 0 || brightnessValue > 100) {
                console.error('Invalid brightness value:', brightness);
                return;
            }

            // 0-100 → 0-255 변환
            const brightness255 = Math.round((brightnessValue * 255) / 100);

            try {
                this.globalBrightness.sending = true;

                const res = await fetch('/api/brightness/broadcast', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({
                        brightness: brightness255
                    })
                });

                if (!res.ok) {
                    const errorData = await res.json().catch(() => ({}));
                    throw new Error(errorData.message || 'Failed to set brightness');
                }

                const data = await res.json();
                if (data.status === 'ok') {
                    this.globalBrightness.value = brightnessValue;
                    this.toast(`Broadcast brightness: ${brightnessValue}%`, 'alert-success');
                } else {
                    throw new Error(data.message || 'Failed to set brightness');
                }
            } catch (e) {
                console.error('Set global brightness error:', e);
                this.toast(e.message, 'alert-error');
            } finally {
                this.globalBrightness.sending = false;
            }
        },

        /**
         * 공지사항 조회 (ESP32 프록시 경유)
         */
        async fetchNotices() {
            try {
                this.notices.loading = true;
                this.notices.error = null;

                const res = await fetch('/api/notices');
                if (!res.ok) {
                    throw new Error('Failed to fetch notices');
                }

                const data = await res.json();
                if (data.success && data.notices) {
                    this.notices.list = data.notices;
                } else {
                    this.notices.list = [];
                }
            } catch (e) {
                console.error('Fetch notices error:', e);
                this.notices.error = 'Failed to load notices';
                this.notices.list = [];
            } finally {
                this.notices.loading = false;
            }
        },

        /**
         * 날짜 포맷 (YYYY-MM-DD)
         */
        formatDate(dateStr) {
            if (!dateStr) return '';
            const date = new Date(dateStr);
            const year = date.getFullYear();
            const month = String(date.getMonth() + 1).padStart(2, '0');
            const day = String(date.getDate()).padStart(2, '0');
            return `${year}-${month}-${day}`;
        },

        /**
         * 공지사항 모달 열기
         */
        openNoticeModal(notice) {
            this.noticeModal.notice = notice;
            this.noticeModal.show = true;
        },

        /**
         * 공지사항 모달 닫기
         */
        closeNoticeModal() {
            this.noticeModal.show = false;
        }
    };
}
