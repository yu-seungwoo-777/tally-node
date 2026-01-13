/**
 * 라이센스 모듈
 * 라이센스 상태 조회 및 검증
 */

export function licenseModule() {
    return {
        // 라이센스 데이터
        license: {
            state: 1,
            stateStr: 'invalid',
            isValid: false,
            deviceLimit: 0,
            key: '',
            loading: false
        },

        // 인터넷 테스트 데이터
        internetTest: {
            status: 'none',      // none, testing, success, fail
            serverStatus: 'none', // none, testing, success, fail
            ping: 0,
            testing: false
        },

        // 라이센스 키 입력 폼
        licenseForm: {
            key: ''
        },

        // 검증 진행 중 플래그
        validating: false,

        // 라이센스 상태 매핑 (C enum license_state_t와 동일)
        stateNames: {
            0: 'invalid',   // LICENSE_STATE_INVALID
            1: 'valid',     // LICENSE_STATE_VALID
            2: 'checking'   // LICENSE_STATE_CHECKING
        },

        /**
         * 초기화
         */
        async initLicense() {
            await this.fetchLicense();
        },

        /**
         * 라이센스 상태 조회 (/api/status에서 가져옴)
         * @param {boolean} showResult - 결과 토스트 표시 여부 (검증 후 사용)
         */
        async fetchLicense(showResult = false) {
            try {
                const res = await fetch('/api/status');
                if (!res.ok) throw new Error('Status fetch failed');

                const data = await res.json();
                const lic = data.license || {};

                this.license.state = lic.state ?? 1;
                this.license.stateStr = lic.stateStr || 'invalid';
                this.license.isValid = lic.isValid || false;
                this.license.deviceLimit = lic.deviceLimit || 0;
                this.license.key = lic.key || '';

                // 활성화된 라이선스 키를 입력 폼에 표시 (포맷팅 포함)
                if (lic.key && lic.key.length === 16) {
                    this.licenseForm.key = this.formatLicenseKeyString(lic.key);
                }

                // 검증 완료 후 결과 토스트 표시
                if (showResult && this.validating) {
                    this.validating = false;
                    if (this.license.isValid) {
                        this.showToast('License validated successfully!', 'alert-success');
                    } else if (this.license.stateStr === 'checking') {
                        this.showToast('License validation in progress...', 'alert-info');
                    } else if (this.license.stateStr === 'invalid') {
                        this.showToast('License validation failed', 'alert-error');
                    }
                }
            } catch (e) {
                console.error('License fetch error:', e);
                this.validating = false;
            }
        },

        /**
         * 라이센스 키 포맷팅 (xxxx-xxxx-xxxx-xxxx)
         */
        formatLicenseKeyString(key) {
            // 하이픈 제거
            let value = key.replace(/-/g, '').toUpperCase();

            // 16자 제한
            if (value.length > 16) {
                value = value.slice(0, 16);
            }

            // 하이픈 추가 (xxxx-xxxx-xxxx-xxxx)
            if (value.length > 12) {
                value = value.slice(0, 4) + '-' + value.slice(4, 8) + '-' + value.slice(8, 12) + '-' + value.slice(12);
            } else if (value.length > 8) {
                value = value.slice(0, 4) + '-' + value.slice(4, 8) + '-' + value.slice(8);
            } else if (value.length > 4) {
                value = value.slice(0, 4) + '-' + value.slice(4);
            }

            return value;
        },

        /**
         * 인터넷 연결 테스트
         */
        async testInternet() {
            this.internetTest.testing = true;
            this.internetTest.status = 'testing';
            this.internetTest.serverStatus = 'testing';
            this.internetTest.ping = 0;

            try {
                // DNS (8.8.8.8) 핑 테스트
                const dnsRes = await fetch('/api/test/internet', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' }
                });

                if (dnsRes.ok) {
                    const dnsData = await dnsRes.json();
                    this.internetTest.status = dnsData.success ? 'success' : 'fail';
                    this.internetTest.ping = dnsData.ping || 0;
                } else {
                    this.internetTest.status = 'fail';
                }

                // 라이센스 서버 연결 테스트
                const serverRes = await fetch('/api/test/license-server', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' }
                });

                if (serverRes.ok) {
                    const serverData = await serverRes.json();
                    this.internetTest.serverStatus = serverData.success ? 'success' : 'fail';
                } else {
                    this.internetTest.serverStatus = 'fail';
                }
            } catch (e) {
                console.error('Internet test error:', e);
                this.internetTest.status = 'fail';
                this.internetTest.serverStatus = 'fail';
            } finally {
                this.internetTest.testing = false;
            }
        },

        /**
         * 라이센스 키 입력 포맷팅 (xxxx-xxxx-xxxx-xxxx)
         */
        formatLicenseKey(event) {
            let value = event.target.value.toUpperCase();

            // 하이픈 제거
            value = value.replace(/-/g, '');

            // 영숫자만 남기기
            value = value.replace(/[^A-Z0-9]/g, '');

            // 16자 제한
            if (value.length > 16) {
                value = value.slice(0, 16);
            }

            // 하이픈 추가 (xxxx-xxxx-xxxx-xxxx)
            if (value.length > 12) {
                value = value.slice(0, 4) + '-' + value.slice(4, 8) + '-' + value.slice(8, 12) + '-' + value.slice(12);
            } else if (value.length > 8) {
                value = value.slice(0, 4) + '-' + value.slice(4, 8) + '-' + value.slice(8);
            } else if (value.length > 4) {
                value = value.slice(0, 4) + '-' + value.slice(4);
            }

            this.licenseForm.key = value;
        },

        /**
         * 라이센스 키 검증 요청
         */
        async validateLicense() {
            const key = this.licenseForm.key.trim();

            if (!key) {
                this.showToast('Please enter license key', 'alert-warning');
                return;
            }

            // 하이픈 제거 (xxxx-xxxx-xxxx-xxxx 형식 지원)
            const cleanKey = key.replace(/-/g, '');

            // 16자리 확인 (백엔드와 동일)
            if (cleanKey.length !== 16) {
                this.showToast('License key must be 16 characters (xxxx-xxxx-xxxx-xxxx)', 'alert-error');
                return;
            }

            try {
                this.license.loading = true;
                this.validating = true;  // 검증 진행 중 플래그 설정

                const res = await fetch('/api/license/validate', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ key: cleanKey })
                });

                if (!res.ok) {
                    throw new Error('Validation request failed');
                }

                const data = await res.json();

                if (data.status === 'accepted') {
                    this.showToast('License validation started. Please wait...', 'alert-info');
                    this.licenseForm.key = '';

                    // 3초 후 상태 갱신 (결과 토스트 표시)
                    setTimeout(() => this.fetchLicense(true), 3000);
                } else {
                    this.showToast('Validation failed: ' + (data.error || 'Unknown error'), 'alert-error');
                }
            } catch (e) {
                console.error('License validate error:', e);
                this.showToast('Validation request failed', 'alert-error');
            } finally {
                this.license.loading = false;
            }
        },

        /**
         * 라이센스 상태 텍스트 반환
         */
        getLicenseStatusText() {
            if (this.license.isValid) {
                return `Active (${this.license.deviceLimit})`;
            }
            const textMap = {
                'invalid': 'Inactive',
                'trial': 'Trial',
                'checking': 'Checking',
                'none': 'Not Registered'
            };
            return textMap[this.license.stateStr] || this.license.stateStr.toUpperCase();
        },

        /**
         * 라이센스 상태 색상 클래스 반환
         */
        getLicenseStatusClass() {
            if (this.license.isValid) return 'text-emerald-600 bg-emerald-50';
            if (this.license.stateStr === 'trial') return 'text-blue-600 bg-blue-50';
            if (this.license.stateStr === 'checking') return 'text-blue-600 bg-blue-50';
            return 'text-rose-600 bg-rose-50';
        },

        /**
         * 라이센스 인디케이터 색상 반환
         */
        getLicenseIndicatorClass() {
            if (this.license.isValid) return 'bg-emerald-500';
            if (this.license.stateStr === 'trial') return 'bg-blue-500';
            if (this.license.stateStr === 'checking') return 'bg-blue-500';
            return 'bg-rose-500';
        }
    };
}
