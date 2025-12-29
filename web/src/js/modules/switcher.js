/**
 * 스위처 모듈
 * Primary/Secondary 스위처 설정, 카메라 매핑
 */

export function switcherModule() {
    return {
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
         * 카메라 버튼 라벨 생성
         */
        getCameraLabel(cameraNum) {
            const offset = this.form.mappingOffset;
            const primary = this.status.switcher?.primary;
            const secondary = this.status.switcher?.secondary;
            const dualEnabled = this.form.switcher.dualEnabled;

            const primaryChannels = primary?.connected ? (primary.tally?.channels || 0) : 0;
            const secondaryChannels = secondary?.connected && dualEnabled
                ? (secondary.tally?.channels || 0)
                : 0;

            // 듀얼 모드 OFF: 모두 Primary
            if (!dualEnabled) {
                if (cameraNum <= primaryChannels) {
                    return `${cameraNum}(P${cameraNum})`;
                }
                return `${cameraNum}`;
            }

            // 듀얼 모드 ON
            const primaryLimited = Math.min(primaryChannels, offset - 1);
            const secondaryStart = offset;

            // Primary 영역
            if (cameraNum <= primaryLimited) {
                return `${cameraNum}(P${cameraNum})`;
            }

            // Secondary 영역
            if (cameraNum >= secondaryStart) {
                const secondaryIndex = cameraNum - secondaryStart + 1;
                if (secondaryIndex <= secondaryChannels) {
                    return `${cameraNum}(S${secondaryIndex})`;
                }
            }

            // 비어있는 영역
            return `${cameraNum}`;
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
        }
    };
}
