/**
 * 네트워크 모듈
 * AP, WiFi STA, Ethernet 설정
 */

export function networkModule() {
    return {
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
                    await this.fetchStatus();
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
                    await this.fetchStatus();
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
                    await this.fetchStatus();
                } else {
                    this.showToast(data.message || 'Failed to save Ethernet settings', 'alert-error');
                }
            } catch (e) {
                this.showToast('Network error. Please try again.', 'alert-error');
            }
        }
    };
}
