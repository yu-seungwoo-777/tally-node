/**
 * Tally Node Web Application
 * Alpine.js 기반 메인 앱
 */

function tallyApp() {
    return {
        // 사이드바 상태
        sidebarOpen: false,

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
            system: 'System'
        },

        // WebSocket 연결 상태
        wsConnected: false,

        // /api/status 응답 데이터 (캐시)
        status: {
            network: { wifi: { connected: false, ssid: '', ip: '--' }, ethernet: { connected: false, ip: '--' } },
            switcher: {
                dualEnabled: false,
                secondaryOffset: 4,
                primary: { connected: false, type: 'ATEM', ip: '', port: 0, tally: { pgm: [], pvw: [], raw: '', channels: 0 } },
                secondary: { connected: false, type: 'ATEM', ip: '', port: 0, tally: { pgm: [], pvw: [], raw: '', channels: 0 } }
            },
            system: { deviceId: '0000', battery: 0, voltage: 0, temperature: 0, uptime: 0 }
        },

        // 네트워크 상태 (UI용 별칭)
        network: {
            wifiConnected: false,
            wifiSsid: '',
            wifiIp: '--',
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
            version: '0.1.0'
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
            device: {
                brightness: 128,
                cameraId: 1,
                rf: {
                    frequency: 868,
                    syncWord: 0x12,
                    spreadingFactor: 7,
                    codingRate: 7,
                    bandwidth: 250,
                    txPower: 22
                }
            }
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
            lora: { syncCode: 18, frequency: 868.0 }
        },

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
            step: 1.0,  // 1MHz 단위 스캔
            results: [],
            recommendation: null,  // 스캔 후 추천
            currentRssi: 0,  // 현재 주파수 RSSI (초기값 0)
            currentStatus: 'unknown'  // clear, busy, unknown
        },

        // 채널 스캔 폴링 타이머
        _channelScanPollingTimer: null,

        // 방송 채널 저장 상태
        broadcast: {
            saving: false
        },

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
         * 뷰 타이틀 계산
         */
        get viewTitle() {
            return this.viewTitles[this.currentView] || 'TALLY-NODE';
        },

        /**
         * 초기화
         */
        async init() {
            console.log('Tally App initializing...');

            // URL 해시에서 현재 뷰 복원
            const hash = window.location.hash.slice(1);
            if (hash && ['dashboard', 'network', 'switcher', 'broadcast', 'system'].includes(hash)) {
                this.currentView = hash;
            }

            // 해시 변경 감지
            window.addEventListener('hashchange', () => {
                const newHash = window.location.hash.slice(1);
                if (newHash && ['dashboard', 'network', 'switcher', 'broadcast', 'system'].includes(newHash)) {
                    this.currentView = newHash;
                }
            });

            await this.fetchStatus();

            // currentView 변경 감지: 스위처 페이지면 폴링 시작
            this.$watch('currentView', (value) => {
                if (value === 'switcher') {
                    this.startStatusPolling();
                } else {
                    this.stopStatusPolling();
                }
            });

            // 초기 로드 시 스위처 페이지면 폴링 시작
            if (this.currentView === 'switcher') {
                this.startStatusPolling();
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
                                password: this.form.ap.password
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
                        }
                    }
                    // Ethernet
                    if (data.network.ethernet) {
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
                }

                // Switcher 업데이트 (primary/secondary에 상태+설정 병합)
                if (data.switcher) {
                    // config는 항상 업데이트 (카드 표시용)
                    this.config.switcher.dualEnabled = data.switcher.dualEnabled || false;
                    this.config.switcher.secondaryOffset = data.switcher.secondaryOffset || 4;

                    // 폼은 초기화 시에만 업데이트 (폴링 시 폼 입력 방해 방지)
                    if (!this._initialized) {
                        this.form.switcher.dualEnabled = data.switcher.dualEnabled || false;
                        this.form.switcher.secondaryOffset = data.switcher.secondaryOffset || 4;
                        this.form.mappingOffset = (data.switcher.secondaryOffset || 4) + 1; // 1-based
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
                }

                // Device 설정 업데이트
                if (data.device) {
                    this.config.device.brightness = data.device.brightness || 128;
                    this.config.device.cameraId = data.device.cameraId || 1;

                    if (data.device.rf) {
                        this.config.device.rf = {
                            frequency: data.device.rf.frequency || 868,
                            syncWord: data.device.rf.syncWord || 0x12,
                            spreadingFactor: data.device.rf.spreadingFactor || 7,
                            codingRate: data.device.rf.codingRate || 7,
                            bandwidth: data.device.rf.bandwidth || 250,
                            txPower: data.device.rf.txPower || 22
                        };
                        // 첫 로드 시 폼 초기화
                        if (!this._initialized) {
                            this.form.display.brightness = data.device.brightness || 128;
                            this.form.display.cameraId = data.device.cameraId || 1;
                            this.form.broadcast.syncCode = data.device.rf.syncWord || 18;
                            this.form.broadcast.frequency = data.device.rf.frequency || 868.0;
                        }
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
            // 이미 실행 중이면 시작 안 함
            if (this._statusPollingTimer) return;

            // 2초마다 상태 조회
            this._statusPollingTimer = setInterval(async () => {
                await this.fetchStatus();
            }, 2000);
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
         * AP 설정 저장
         */
        async saveAp() {
            try {
                const res = await fetch('/api/config/network/ap', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({
                        ssid: this.form.ap.ssid,
                        password: this.form.ap.password,
                        channel: this.form.ap.channel,
                        enabled: this.form.ap.enabled
                    })
                });
                const data = await res.json();
                if (data.status === 'ok') {
                    this.showToast('WiFi AP settings saved - Reconnect if IP changed', 'alert-success');
                    await this.fetchStatus();  // 상태 갱신
                } else {
                    this.showToast(data.message || 'Failed to save WiFi AP settings', 'alert-error');
                }
            } catch (e) {
                this.showToast('Network error. Please try again.', 'alert-error');
            }
        },

        /**
         * WiFi STA 설정 저장
         */
        async saveWifi() {
            try {
                const res = await fetch('/api/config/network/wifi', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({
                        ssid: this.form.wifi.ssid,
                        password: this.form.wifi.password,
                        enabled: this.form.wifi.enabled
                    })
                });
                const data = await res.json();
                if (data.status === 'ok') {
                    this.showToast('WiFi STA settings saved - Reconnect if IP changed', 'alert-success');
                    await this.fetchStatus();  // 상태 갱신
                } else {
                    this.showToast(data.message || 'Failed to save WiFi STA settings', 'alert-error');
                }
            } catch (e) {
                this.showToast('Network error. Please try again.', 'alert-error');
            }
        },

        /**
         * Ethernet 설정 저장
         */
        async saveEthernet() {
            try {
                const res = await fetch('/api/config/network/ethernet', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({
                        dhcp: this.form.ethernet.dhcp,
                        staticIp: this.form.ethernet.ip,
                        gateway: this.form.ethernet.gateway,
                        netmask: this.form.ethernet.netmask,
                        enabled: this.form.ethernet.enabled
                    })
                });
                const data = await res.json();
                if (data.status === 'ok') {
                    this.showToast('Ethernet settings saved - Reconnect if IP changed', 'alert-success');
                    await this.fetchStatus();  // 상태 갱신
                } else {
                    this.showToast(data.message || 'Failed to save Ethernet settings', 'alert-error');
                }
            } catch (e) {
                this.showToast('Network error. Please try again.', 'alert-error');
            }
        },

        /**
         * Primary Switcher 설정 저장
         */
        async savePrimarySwitcher() {
            try {
                const payload = {
                    type: this.form.switcher.primary.type,
                    ip: this.form.switcher.primary.ip,
                    port: this.form.switcher.primary.port,
                    cameraLimit: this.form.switcher.primary.cameraLimit
                };
                // ATEM만 interface 지원
                if (this.form.switcher.primary.type === 'ATEM') {
                    payload.interface = this.form.switcher.primary.interface;
                }
                // OBS만 password 지원
                if (this.form.switcher.primary.type === 'OBS') {
                    payload.password = this.form.switcher.primary.password || '';
                }
                const res = await fetch('/api/config/switcher/primary', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify(payload)
                });
                const data = await res.json();
                if (data.status === 'ok') {
                    this.showToast(`Primary switcher saved: ${this.form.switcher.primary.type} @ ${this.form.switcher.primary.ip || '--'}`, 'alert-success');
                    this.showPrimaryConfig = false;
                    await this.fetchStatus();
                } else {
                    this.showToast(data.message || 'Failed to save primary switcher', 'alert-error');
                }
            } catch (e) {
                this.showToast('Network error. Please try again.', 'alert-error');
            }
        },

        /**
         * Secondary Switcher 설정 저장
         */
        async saveSecondarySwitcher() {
            try {
                const payload = {
                    type: this.form.switcher.secondary.type,
                    ip: this.form.switcher.secondary.ip,
                    port: this.form.switcher.secondary.port,
                    cameraLimit: this.form.switcher.secondary.cameraLimit
                };
                // ATEM만 interface 지원
                if (this.form.switcher.secondary.type === 'ATEM') {
                    payload.interface = this.form.switcher.secondary.interface;
                }
                // OBS만 password 지원
                if (this.form.switcher.secondary.type === 'OBS') {
                    payload.password = this.form.switcher.secondary.password || '';
                }
                const res = await fetch('/api/config/switcher/secondary', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify(payload)
                });
                const data = await res.json();
                if (data.status === 'ok') {
                    this.showToast(`Secondary switcher saved: ${this.form.switcher.secondary.type} @ ${this.form.switcher.secondary.ip || '--'}`, 'alert-success');
                    this.showSecondaryConfig = false;
                    await this.fetchStatus();
                } else {
                    this.showToast(data.message || 'Failed to save secondary switcher', 'alert-error');
                }
            } catch (e) {
                this.showToast('Network error. Please try again.', 'alert-error');
            }
        },

        /**
         * 스위처 타입별 기본 포트 반환
         */
        getDefaultPort(type) {
            const ports = { ATEM: 9910, vMix: 8099, OBS: 4455, OSEE: 9999 };
            return ports[type] || 9910;
        },

        /**
         * 스위처 타입별 표시 이름 반환
         */
        getSwitcherTypeDisplayName(type) {
            const names = {
                ATEM: 'ATEM',
                vMix: 'vMix',
                OBS: 'OBS (In Development)',
                OSEE: 'OSEE (In Development)'
            };
            return names[type] || type;
        },

        /**
         * 스위처 타입별 설명 반환
         */
        getSwitcherTypeDesc(type) {
            const descs = {
                ATEM: 'Blackmagic Design ATEM Switcher',
                vMix: 'vMix Live Video Production Software',
                OBS: 'OBS Studio (In Development)',
                OSEE: 'OSEE (In Development)'
            };
            return descs[type] || '';
        },

        /**
         * 인터페이스 값에 따른 이름 반환
         */
        getInterfaceName(val) {
            const names = { 0: 'Auto', 1: 'Ethernet', 2: 'WiFi' };
            return names[val] || 'Auto';
        },

        /**
         * 스위처 타입 변경 시 처리 (포트 자동 변경)
         */
        onSwitcherTypeChange(role) {
            const sw = this.form.switcher[role];
            sw.port = this.getDefaultPort(sw.type);
            // ATEM이 아닐 때 interface는 Auto(0)로 설정
            if (sw.type !== 'ATEM') {
                sw.interface = 0;
            }
        },

        /**
         * 듀얼 모드 변경 시 처리
         */
        async onDualModeChange() {
            // 듀얼 모드 활성화 시 offset 초기화
            if (this.form.switcher.dualEnabled) {
                this.form.mappingOffset = this.config.switcher.secondaryOffset + 1;
            }

            // 듀얼 모드 설정 저장
            try {
                const res = await fetch('/api/config/switcher/dual', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({
                        dualEnabled: this.form.switcher.dualEnabled
                    })
                });
                const data = await res.json();
                if (data.status === 'ok') {
                    this.showToast(
                        this.form.switcher.dualEnabled
                            ? 'Dual mode enabled - Configure secondary start position'
                            : 'Dual mode disabled',
                        'alert-success'
                    );
                    await this.fetchStatus();
                } else {
                    this.showToast(data.message || 'Failed to save dual mode', 'alert-error');
                    // 실패 시 토글 상태 복원
                    this.form.switcher.dualEnabled = !this.form.switcher.dualEnabled;
                }
            } catch (e) {
                this.showToast('Network error. Please try again.', 'alert-error');
                // 실패 시 토글 상태 복원
                this.form.switcher.dualEnabled = !this.form.switcher.dualEnabled;
            }
        },

        /**
         * 카메라 버튼 스타일 계산
         *
         * 듀얼 모드 OFF: Primary 전체 표시
         * 듀얼 모드 ON: Primary(offset까지) + Secondary(offset부터)
         */
        getCameraButtonClass(cameraNum) {
            const offset = this.form.mappingOffset;
            const primary = this.status.switcher?.primary;
            const secondary = this.status.switcher?.secondary;
            const dualEnabled = this.form.switcher.dualEnabled;

            const primaryChannels = primary?.connected ? (primary.tally?.channels || 0) : 0;
            const secondaryChannels = secondary?.connected && dualEnabled
                ? (secondary.tally?.channels || 0)
                : 0;

            // 듀얼 모드 OFF: Primary만 전체 표시
            if (!dualEnabled) {
                if (cameraNum <= primaryChannels) {
                    return 'bg-blue-500 text-white cursor-pointer hover:bg-blue-600';
                }
                return 'bg-slate-100 text-slate-400 cursor-pointer hover:bg-slate-200';
            }

            // 듀얼 모드 ON: offset으로 분할
            const primaryLimited = Math.min(primaryChannels, offset - 1);
            const secondaryEnd = Math.min(offset + secondaryChannels - 1, 20);

            // Primary 영역
            if (cameraNum <= primaryLimited) {
                return 'bg-blue-500 text-white cursor-pointer hover:bg-blue-600';
            }

            // Primary와 Secondary 사이 빈 공간
            if (cameraNum < offset) {
                return 'bg-slate-100 text-slate-400 cursor-pointer hover:bg-slate-200';
            }

            // Secondary 시작 위치 (offset)
            if (cameraNum === offset) {
                return secondaryChannels > 0
                    ? 'bg-purple-700 text-white ring-2 ring-purple-400 ring-offset-2 cursor-pointer hover:bg-purple-800'
                    : 'bg-slate-300 text-slate-600 cursor-pointer hover:bg-slate-400';
            }

            // Secondary 영역
            if (cameraNum <= secondaryEnd) {
                return 'bg-purple-500 text-white cursor-pointer hover:bg-purple-600';
            }

            return 'bg-slate-100 text-slate-400 cursor-pointer hover:bg-slate-200';
        },

        /**
         * Offset 선택
         */
        selectOffset(cameraNum) {
            if (!this.form.switcher.dualEnabled) return;
            this.form.mappingOffset = cameraNum;
        },

        /**
         * 매핑 저장 (Secondary offset)
         */
        async saveMapping() {
            try {
                const res = await fetch('/api/config/switcher/dual', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({
                        dualEnabled: this.form.switcher.dualEnabled,
                        secondaryOffset: this.form.mappingOffset - 1  // 0-based로 변환
                    })
                });
                const data = await res.json();
                if (data.status === 'ok') {
                    this.showToast(`Mapping saved: Secondary starts at camera ${this.form.mappingOffset}`, 'alert-success');
                    await this.fetchStatus();
                } else {
                    this.showToast(data.message || 'Failed to save mapping', 'alert-error');
                }
            } catch (e) {
                this.showToast('Network error. Please try again.', 'alert-error');
            }
        },

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
                        frequency: this.form.broadcast.frequency,
                        syncWord: this.form.broadcast.syncCode
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
                this.channelScan.recommendation = null;  // 추천 초기화

                const res = await fetch('/api/broadcast/scan/start', {
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
                await fetch('/api/broadcast/scan/stop', { method: 'POST' });
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
                const res = await fetch('/api/broadcast/scan');
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
                    // 현재 채널이 조용함
                    if (currentResult.frequency === quietest.frequency) {
                        // 이미 가장 조용한 채널
                        this.channelScan.recommendation = {
                            type: 'good',
                            title: 'Current frequency is optimal',
                            message: `Current channel is clear (${currentResult.rssi} dBm)`
                        };
                    } else {
                        // 더 조용한 채널 있음
                        const improvement = quietestRssi - currentResult.rssi;
                        this.channelScan.recommendation = {
                            type: 'better',
                            title: 'Quieter channel available',
                            message: `${quietest.frequency.toFixed(1)} MHz is ${improvement.toFixed(1)} dBm quieter`,
                            suggestedFreq: quietest.frequency
                        };
                    }
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
         * -90dBm 이하: 초록, -90~-80: 노랑, -80 이상: 빨강
         */
        getGaugeColor(rssi) {
            if (rssi <= -90) return '#22c55e';  // green-500
            if (rssi <= -80) return '#eab308';  // yellow-500
            return '#ef4444';  // red-500
        },

        /**
         * RSSI 게이지 stroke-dasharray 계산
         * rcl(2π * r * percentage)
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
            // 추천 갱신
            this.analyzeScanRecommendation();
            this.showToast(`Channel selected: ${Math.round(freq)} MHz - Click "Save Channel Settings" to apply`, 'alert-info');
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
        }
    };
}
