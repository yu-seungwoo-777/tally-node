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
            graceRemaining: 0,
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

        // 라이센스 상태 매핑
        stateNames: {
            0: 'none',
            1: 'invalid',
            2: 'trial',
            3: 'valid',
            4: 'grace',
            5: 'expired'
        },

        /**
         * 초기화
         */
        async initLicense() {
            await this.fetchLicense();
        },

        /**
         * 라이센스 상태 조회
         */
        async fetchLicense() {
            try {
                const res = await fetch('/api/license');
                if (!res.ok) throw new Error('License fetch failed');

                const data = await res.json();

                this.license.state = data.state || 1;
                this.license.stateStr = data.stateStr || 'invalid';
                this.license.isValid = data.isValid || false;
                this.license.deviceLimit = data.deviceLimit || 0;
                this.license.graceRemaining = data.graceRemaining || 0;
                this.license.key = data.key || '';
            } catch (e) {
                console.error('License fetch error:', e);
            }
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
         * 라이센스 키 검증 요청
         */
        async validateLicense() {
            const key = this.licenseForm.key.trim();

            if (!key) {
                this.showToast('Please enter license key', 'alert-warning');
                return;
            }

            // 16자리 확인
            if (key.length < 8) {
                this.showToast('Invalid license key format', 'alert-error');
                return;
            }

            try {
                this.license.loading = true;

                const res = await fetch('/api/license/validate', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ key })
                });

                if (!res.ok) {
                    throw new Error('Validation request failed');
                }

                const data = await res.json();

                if (data.status === 'accepted') {
                    this.showToast('License validation started. Please wait...', 'alert-info');
                    this.licenseForm.key = '';

                    // 3초 후 상태 갱신
                    setTimeout(() => this.fetchLicense(), 3000);
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
                'grace': 'Grace Period',
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
            if (this.license.stateStr === 'grace') return 'text-amber-600 bg-amber-50';
            if (this.license.stateStr === 'checking') return 'text-blue-600 bg-blue-50';
            return 'text-rose-600 bg-rose-50';
        },

        /**
         * 라이센스 인디케이터 색상 반환
         */
        getLicenseIndicatorClass() {
            if (this.license.isValid) return 'bg-emerald-500';
            if (this.license.stateStr === 'trial') return 'bg-blue-500';
            if (this.license.stateStr === 'grace') return 'bg-amber-500';
            if (this.license.stateStr === 'checking') return 'bg-blue-500';
            return 'bg-rose-500';
        }
    };
}
