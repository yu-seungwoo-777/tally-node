// API 엔드포인트
const API = {
    STATUS: '/api/status',
    CONFIG: '/api/config',
    WIFI: '/api/config/wifi',
    WIFI_SCAN: '/api/wifi/scan',
    ETH: '/api/config/eth',
    SWITCHERS: '/api/config/switchers',
    SWITCHER: '/api/config/switcher',
    SWITCHER_MAPPING: '/api/config/switcher/mapping',
    MODE: '/api/config/mode',
    HEALTH: '/api/system/health',
    RESTART: '/api/restart',
    SWITCHER_RESTART: '/api/switcher/restart',
    LORA_STATUS: '/api/lora/status',
    LORA_SCAN: '/api/lora/scan',
    LORA_CONFIG: '/api/lora/config'
};

// 상태 업데이트 주기 (ms)
const UPDATE_INTERVAL = 3000;
const HEALTH_UPDATE_INTERVAL = 5000;

// 전역 변수
let updateTimer = null;
let healthTimer = null;
let switcherData = null;  // API 로드 전까지 null
let secondaryStartPos = 1;

// 초기화
document.addEventListener('DOMContentLoaded', () => {
    console.log('EoRa-S3 Web UI 초기화...');

    // 초기 데이터 로드
    loadConfig();
    loadSwitcherConfig();
    updateStatus();
    updateHealth();
    updateLoRaStatus();

    // 주기적 상태 업데이트
    updateTimer = setInterval(updateStatus, UPDATE_INTERVAL);
    healthTimer = setInterval(updateHealth, HEALTH_UPDATE_INTERVAL);

    // 이벤트 리스너 등록
    setupEventListeners();
});

// 이벤트 리스너 설정
function setupEventListeners() {
    // WiFi 스캔 버튼
    document.getElementById('wifi-scan-btn').addEventListener('click', handleWiFiScan);

    // WiFi STA 폼
    document.getElementById('wifi-sta-form').addEventListener('submit', handleWiFiSubmit);

    // Ethernet 폼
    document.getElementById('eth-form').addEventListener('submit', handleEthSubmit);

    // 모드 저장 버튼
    document.getElementById('save-mode-btn').addEventListener('click', handleModeChange);

    // 스위처 연결 재시작 버튼
    document.getElementById('restart-switcher-btn').addEventListener('click', handleSwitcherRestart);

    // 매핑 저장 버튼
    document.getElementById('save-mapping-btn').addEventListener('click', handleMappingSave);

    // 재시작 버튼
    document.getElementById('restart-btn').addEventListener('click', handleRestart);

    // LoRa 스캔 버튼
    document.getElementById('lora-scan-btn').addEventListener('click', handleLoRaScan);

    // LoRa 주파수 변경 버튼
    document.getElementById('lora-freq-save-btn').addEventListener('click', handleLoRaFrequencyChange);

    // LoRa Sync Word 변경 버튼
    document.getElementById('lora-sync-save-btn').addEventListener('click', handleLoRaSyncWordChange);

    // LoRa Sync Word 슬라이더 값 변경 시 표시 업데이트
    document.getElementById('lora-sync-input').addEventListener('input', (e) => {
        const value = parseInt(e.target.value);
        document.getElementById('lora-sync-value').textContent = '0x' + value.toString(16).toUpperCase().padStart(2, '0');
    });

    // LoRa 설정 적용 버튼
    document.getElementById('lora-apply-btn').addEventListener('click', handleLoRaApply);
}

// 설정 로드
async function loadConfig() {
    try {
        const response = await fetch(API.CONFIG);
        if (!response.ok) throw new Error('설정 로드 실패');

        const config = await response.json();

        // WiFi STA 설정
        document.getElementById('wifi-ssid').value = config.wifi_sta.ssid || '';
        document.getElementById('wifi-password').value = config.wifi_sta.password || '';

        // Ethernet 설정
        document.getElementById('eth-dhcp').checked = config.eth.dhcp_enabled;
        document.getElementById('eth-ip').value = config.eth.static_ip || '';
        document.getElementById('eth-netmask').value = config.eth.static_netmask || '';
        document.getElementById('eth-gateway').value = config.eth.static_gateway || '';

    } catch (error) {
        console.error('설정 로드 오류:', error);
        showMessage('설정을 불러오는데 실패했습니다.', 'error');
    }
}

// 상태 업데이트
async function updateStatus() {
    try {
        const response = await fetch(API.STATUS);
        if (!response.ok) throw new Error('상태 조회 실패');

        const status = await response.json();

        // WiFi AP 상태
        updateWiFiAPStatus(status.wifi_ap, status.wifi_detail);

        // WiFi STA 상태
        updateWiFiSTAStatus(status.wifi_sta, status.wifi_detail);

        // Ethernet 상태
        updateEthStatus(status.ethernet, status.eth_detail);

        // 스위처 상태 (switcher 필드가 있으면)
        if (status.switcher) {
            updateSwitcherStatus(status.switcher);
            // 개별 스위처 상태도 업데이트
            if (status.switcher.primary) {
                updateSwitcherItem(0, status.switcher.primary);
            }
            if (status.switcher.secondary) {
                updateSwitcherItem(1, status.switcher.secondary);
            }
        }

        // LoRa 상태 (lora 필드가 있으면)
        if (status.lora) {
            updateLoRaStatusFromData(status.lora);
        }

        // 전체 연결 상태
        updateConnectionBadge(status);

    } catch (error) {
        console.error('상태 업데이트 오류:', error);
        document.getElementById('status-badge').textContent = '연결 끊김';
        document.getElementById('status-badge').className = 'badge';
    }
}

// WiFi AP 상태 업데이트
function updateWiFiAPStatus(status, detail) {
    const html = `
        <div class="value">${status.active ? '활성' : '비활성'}</div>
        ${status.active ? `
            <div class="label">IP: ${status.ip}</div>
            <div class="label">클라이언트: ${detail.ap_clients}개</div>
        ` : ''}
    `;
    document.getElementById('wifi-ap-status').innerHTML = html;
}

// WiFi STA 상태 업데이트
function updateWiFiSTAStatus(status, detail) {
    const html = `
        <div class="value">${status.connected ? '연결됨' : '연결 안됨'}</div>
        ${status.connected ? `
            <div class="label">IP: ${status.ip}</div>
            <div class="label">신호: ${detail.sta_rssi} dBm</div>
        ` : ''}
    `;
    document.getElementById('wifi-sta-status').innerHTML = html;
}

// Ethernet 상태 업데이트
function updateEthStatus(status, detail) {
    const html = `
        <div class="value">${status.active ? (status.connected ? '연결됨' : '연결 안됨') : '비활성'}</div>
        ${status.active && status.connected ? `
            <div class="label">IP: ${status.ip}</div>
            <div class="label">모드: ${detail.dhcp_mode ? 'DHCP' : 'Static'}</div>
            <div class="label">MAC: ${detail.mac}</div>
        ` : (status.active ? '<div class="label">케이블 확인</div>' : '<div class="label">W5500 없음</div>')}
    `;
    document.getElementById('eth-status').innerHTML = html;
}

// 스위처 상태 업데이트
function updateSwitcherStatus(switcher) {
    // 동작 모드
    const modeEl = document.getElementById('switcher-mode');
    if (modeEl) {
        if (!switcher.comm_initialized) {
            modeEl.textContent = '초기화 중...';
            modeEl.className = 'badge connecting';
        } else if (switcher.dual_mode) {
            modeEl.textContent = '듀얼 모드';
            modeEl.className = 'badge connected';
        } else {
            modeEl.textContent = '싱글 모드';
            modeEl.className = 'badge';
        }
    }

    // 활성 스위처 개수
    const countEl = document.getElementById('switcher-active-count');
    if (countEl) {
        countEl.textContent = `${switcher.active_count}개`;
    }

    // /api/status에는 program/preview만 있음
    // effective, offset 등은 /api/config/switchers에서 가져옴

    // Primary 스위처
    updateSwitcherItem(0, switcher.primary);

    // Secondary 스위처
    updateSwitcherItem(1, switcher.secondary);
}

// 개별 스위처 상태 업데이트
function updateSwitcherItem(index, sw) {
    const badgeEl = document.getElementById(`sw${index}-status-badge`);
    const infoEl = document.getElementById(`sw${index}-connection-info`);

    if (!badgeEl || !infoEl) return;

    if (sw.connected) {
        badgeEl.textContent = '연결됨';
        badgeEl.className = 'badge connected';

        const info = [];
        if (sw.product) info.push(`제품: ${sw.product}`);
        if (sw.num_cameras) info.push(`카메라: ${sw.num_cameras}개`);
        if (sw.program !== undefined) info.push(`PGM: ${sw.program}`);
        if (sw.preview !== undefined) info.push(`PVW: ${sw.preview}`);
        if (sw.offset !== undefined) info.push(`Offset: ${sw.offset}`);
        if (sw.limit !== undefined) info.push(`Limit: ${sw.limit === 0 ? '무제한' : sw.limit}`);
        if (sw.effective !== undefined) info.push(`Effective: ${sw.effective}`);

        infoEl.innerHTML = `<div class="label">${info.join(' | ')}</div>`;
    } else {
        badgeEl.textContent = '연결 안됨';
        badgeEl.className = 'badge';
        infoEl.innerHTML = '';
    }
}

// 연결 상태 배지 업데이트
function updateConnectionBadge(status) {
    const badge = document.getElementById('status-badge');

    if (status.wifi_sta.connected || status.ethernet.connected) {
        badge.textContent = '연결됨';
        badge.className = 'badge connected';
    } else {
        badge.textContent = 'AP 모드';
        badge.className = 'badge connecting';
    }
}

// WiFi 스캔 처리
async function handleWiFiScan() {
    const btn = document.getElementById('wifi-scan-btn');
    const status = document.getElementById('wifi-scan-status');
    const resultsDiv = document.getElementById('wifi-scan-results');

    try {
        btn.disabled = true;
        status.textContent = '검색 중...';
        resultsDiv.style.display = 'none';

        const response = await fetch(API.WIFI_SCAN);
        if (!response.ok) throw new Error('WiFi 스캔 실패');

        const data = await response.json();

        if (data.count === 0) {
            status.textContent = '검색된 네트워크가 없습니다.';
            return;
        }

        status.textContent = `${data.count}개 네트워크 발견`;

        // 스캔 결과 표시
        resultsDiv.innerHTML = '';
        data.networks.forEach(network => {
            const networkItem = document.createElement('div');
            networkItem.className = 'wifi-network-item';
            networkItem.innerHTML = `
                <div class="network-info">
                    <strong>${network.ssid}</strong>
                    <span class="network-auth">${network.auth}</span>
                </div>
                <div class="network-details">
                    <span>신호: ${network.rssi} dBm</span>
                    <span>채널: ${network.channel}</span>
                </div>
            `;
            networkItem.addEventListener('click', () => {
                document.getElementById('wifi-ssid').value = network.ssid;
                document.getElementById('wifi-password').focus();
            });
            resultsDiv.appendChild(networkItem);
        });

        resultsDiv.style.display = 'block';

    } catch (error) {
        console.error('WiFi 스캔 오류:', error);
        status.textContent = '스캔 실패';
        showMessage('WiFi 스캔에 실패했습니다.', 'error');
    } finally {
        btn.disabled = false;
    }
}

// WiFi 설정 제출
async function handleWiFiSubmit(e) {
    e.preventDefault();

    const formData = new FormData(e.target);
    const data = {
        ssid: formData.get('ssid'),
        password: formData.get('password')
    };

    try {
        const response = await fetch(API.WIFI, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(data)
        });

        if (!response.ok) throw new Error('WiFi 설정 실패');

        showMessage('WiFi 설정이 저장되고 재시작됩니다.', 'success');

    } catch (error) {
        console.error('WiFi 설정 오류:', error);
        showMessage('WiFi 설정 저장에 실패했습니다.', 'error');
    }
}

// Ethernet 설정 제출
async function handleEthSubmit(e) {
    e.preventDefault();

    const formData = new FormData(e.target);
    const data = {
        dhcp_enabled: document.getElementById('eth-dhcp').checked,
        static_ip: formData.get('static_ip'),
        static_netmask: formData.get('static_netmask'),
        static_gateway: formData.get('static_gateway')
    };

    try {
        const response = await fetch(API.ETH, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(data)
        });

        if (!response.ok) throw new Error('Ethernet 설정 실패');

        showMessage('Ethernet 설정이 저장되었습니다. 재시작 후 적용됩니다.', 'success');

    } catch (error) {
        console.error('Ethernet 설정 오류:', error);
        showMessage('Ethernet 설정 저장에 실패했습니다.', 'error');
    }
}

// 모드 변경 처리
async function handleModeChange() {
    const mode = document.getElementById('switcher-mode-select').value;
    const isDual = (mode === 'dual');

    try {
        // 단순히 dual_mode 플래그만 변경
        await fetch(API.MODE, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ dual_mode: isDual })
        });

        showMessage(`${isDual ? '듀얼' : '싱글'} 모드로 설정되었습니다. 스위처 연결 재시작이 필요합니다.`, 'success');

        // 설정 다시 로드
        setTimeout(() => {
            loadSwitcherConfig();
        }, 500);

    } catch (error) {
        console.error('모드 변경 오류:', error);
        showMessage('모드 변경에 실패했습니다.', 'error');
    }
}

// 스위처 연결 재시작
async function handleSwitcherRestart() {
    try {
        showMessage('스위처 연결을 재시작합니다...', 'info');

        const response = await fetch(API.SWITCHER_RESTART, { method: 'POST' });
        if (!response.ok) throw new Error('스위처 재시작 실패');

        // 연결 대기
        const result = await waitForSwitcherConnection(10);
        showMessage(result.message, result.success ? 'success' : 'error');

    } catch (error) {
        console.error('스위처 재시작 오류:', error);
        showMessage('스위처 재시작에 실패했습니다.', 'error');
    }
}

// 시스템 재시작
async function handleRestart() {
    if (!confirm('시스템을 재시작하시겠습니까?')) return;

    try {
        const response = await fetch(API.RESTART, { method: 'POST' });
        if (!response.ok) throw new Error('재시작 실패');

        clearInterval(updateTimer);
        showMessage('시스템을 재시작합니다...', 'success');

        // 20초 후 페이지 새로고침
        setTimeout(() => {
            window.location.reload();
        }, 20000);

    } catch (error) {
        console.error('재시작 오류:', error);
        showMessage('재시작에 실패했습니다.', 'error');
    }
}


// 스위처 설정 로드
async function loadSwitcherConfig() {
    try {
        const response = await fetch(API.SWITCHERS);
        if (!response.ok) throw new Error('스위처 설정 로드 실패');

        const data = await response.json();
        const switchers = data.switchers;

        // switcherData 초기화 및 설정 저장
        switcherData = {
            switchers: switchers,  // 전체 스위처 설정 저장
            primary: { limit: 0, offset: 0, effective: 0 },
            secondary: { limit: 0, offset: 0, effective: 0 }
        };

        // 모드 선택 설정 (dual_mode 플래그 사용)
        const isDualMode = data.dual_mode || false;
        document.getElementById('switcher-mode-select').value = isDualMode ? 'dual' : 'single';

        // 매핑 데이터 저장
        switchers.forEach(sw => {
            if (sw.index === 0) {
                switcherData.primary.limit = sw.camera_limit || 0;
                switcherData.primary.offset = sw.camera_offset || 0;
                switcherData.primary.effective = sw.effective_count || 0;
            } else if (sw.index === 1) {
                switcherData.secondary.limit = sw.camera_limit || 0;
                switcherData.secondary.offset = sw.camera_offset || 0;
                switcherData.secondary.effective = sw.effective_count || 0;
            }
        });

        // Secondary 시작 위치 설정 (서버에서 받은 offset 사용)
        const secondaryOffset = switcherData.secondary.offset || 0;
        secondaryStartPos = secondaryOffset + 1;

        // 비주얼 매핑 UI 생성
        renderCameraMapping();

    } catch (error) {
        console.error('스위처 설정 로드 오류:', error);
        showMessage('스위처 설정을 불러오는데 실패했습니다.', 'error');
    }
}

// 스위처 재시작 후 연결 대기
async function waitForSwitcherConnection(countdown = 10) {
    return new Promise((resolve) => {
        let timeLeft = countdown;
        const checkInterval = 1000; // 1초마다 체크

        const checkConnection = async () => {
            try {
                const response = await fetch(API.STATUS);
                if (response.ok) {
                    const status = await response.json();

                    if (status.switcher) {
                        const isDualMode = status.switcher.dual_mode;
                        const primaryConnected = status.switcher.primary?.connected || false;
                        const secondaryConnected = status.switcher.secondary?.connected || false;

                        // 연결 상태 체크
                        if (isDualMode) {
                            // 듀얼 모드: 둘 다 연결되어야 함
                            if (primaryConnected && secondaryConnected) {
                                resolve({ success: true, message: '모든 스위처가 연결되었습니다.' });
                                return;
                            }
                        } else {
                            // 싱글 모드: Primary만 연결되면 됨
                            if (primaryConnected) {
                                resolve({ success: true, message: '스위처가 연결되었습니다.' });
                                return;
                            }
                        }

                        // 시간 초과 체크
                        if (timeLeft <= 0) {
                            let failMessage = '';
                            if (isDualMode) {
                                if (!primaryConnected && !secondaryConnected) {
                                    failMessage = '두 스위처 모두 연결에 실패했습니다.';
                                } else if (!primaryConnected) {
                                    failMessage = 'Primary 스위처 연결에 실패했습니다.';
                                } else if (!secondaryConnected) {
                                    failMessage = 'Secondary 스위처 연결에 실패했습니다.';
                                }
                            } else {
                                failMessage = 'Primary 스위처 연결에 실패했습니다.';
                            }
                            resolve({ success: false, message: failMessage });
                            return;
                        }

                        // 카운트다운 메시지 업데이트
                        showMessage(`스위처 재시작 중... (${timeLeft}초)`, 'info');
                        timeLeft--;
                        setTimeout(checkConnection, checkInterval);
                    }
                }
            } catch (error) {
                if (timeLeft <= 0) {
                    resolve({ success: false, message: '스위처 연결 상태를 확인할 수 없습니다.' });
                } else {
                    timeLeft--;
                    setTimeout(checkConnection, checkInterval);
                }
            }
        };

        checkConnection();
    });
}

// 스위처 설정 제출
async function handleSwitcherSubmit(e) {
    e.preventDefault();

    const form = e.target;
    const index = parseInt(form.dataset.index);
    const formData = new FormData(form);

    const data = {
        index: index,
        type: parseInt(formData.get('type')),
        interface: parseInt(formData.get('interface')),
        ip: formData.get('ip'),
        password: formData.get('password')
        // port는 서버에서 타입에 따라 자동 설정됨
    };

    try {
        const response = await fetch(API.SWITCHER, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(data)
        });

        if (!response.ok) throw new Error('스위처 설정 실패');

        showMessage(`스위처 ${index === 0 ? 'Primary' : 'Secondary'} 설정이 저장되었습니다. 스위처 연결 재시작이 필요합니다.`, 'success');

        // 설정 다시 로드
        setTimeout(() => {
            loadSwitcherConfig();
        }, 500);

    } catch (error) {
        console.error('스위처 설정 오류:', error);
        showMessage('스위처 설정 저장에 실패했습니다.', 'error');
    }
}

// 카메라 제한 저장
async function saveCameraLimit(index, limit) {
    const data = {
        index: index,
        camera_limit: limit,
        camera_offset: index === 0 ? 0 : (secondaryStartPos - 1)
    };

    const response = await fetch(API.SWITCHER_MAPPING, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(data)
    });

    if (!response.ok) throw new Error('카메라 제한 저장 실패');
}

// Health 상태 업데이트
async function updateHealth() {
    try {
        const response = await fetch(API.HEALTH);
        if (!response.ok) throw new Error('Health API 요청 실패');

        const data = await response.json();

        // 전체 상태
        const overallEl = document.getElementById('health-overall');
        overallEl.className = 'health-value';
        overallEl.classList.add(`status-${data.status}`);
        switch(data.status) {
            case 'ok':
                overallEl.textContent = '✓ 정상';
                break;
            case 'warning':
                overallEl.textContent = '⚠ 경고';
                break;
            case 'critical':
                overallEl.textContent = '✗ 위험';
                break;
            default:
                overallEl.textContent = '알 수 없음';
        }

        // 업타임
        const uptimeEl = document.getElementById('health-uptime');
        const uptime = formatUptime(data.uptime_sec);
        uptimeEl.textContent = uptime;
        uptimeEl.className = 'health-value';

        // 전압
        const voltageEl = document.getElementById('health-voltage');
        if (data.voltage && data.voltage.volts !== undefined) {
            voltageEl.textContent = data.voltage.volts.toFixed(2) + ' V';
            voltageEl.className = 'health-value';
            voltageEl.classList.add(`status-${data.voltage.status}`);
        } else {
            voltageEl.textContent = '사용 불가';
            voltageEl.className = 'health-value status-unavailable';
        }

        // 배터리
        const batteryEl = document.getElementById('health-battery');
        if (data.voltage && data.voltage.percentage !== undefined) {
            batteryEl.textContent = data.voltage.percentage + ' %';
            batteryEl.className = 'health-value';
            batteryEl.classList.add(`status-${data.voltage.status}`);
        } else {
            batteryEl.textContent = '사용 불가';
            batteryEl.className = 'health-value status-unavailable';
        }

        // 메모리
        const memoryEl = document.getElementById('health-memory');
        if (data.memory) {
            const freeKB = (data.memory.free_heap_bytes / 1024).toFixed(1);
            memoryEl.textContent = freeKB + ' KB';
            memoryEl.className = 'health-value';
            memoryEl.classList.add(`status-${data.memory.status}`);
        } else {
            memoryEl.textContent = '사용 불가';
            memoryEl.className = 'health-value status-unavailable';
        }

        // 온도
        const tempEl = document.getElementById('health-temperature');
        if (data.temperature && data.temperature.celsius !== undefined) {
            tempEl.textContent = data.temperature.celsius.toFixed(1) + ' °C';
            tempEl.className = 'health-value';
            tempEl.classList.add(`status-${data.temperature.status}`);
        } else {
            tempEl.textContent = '사용 불가';
            tempEl.className = 'health-value status-unavailable';
        }

        // CPU
        const cpuEl = document.getElementById('health-cpu');
        if (data.cpu && data.cpu.core0_usage_percent !== undefined && data.cpu.core1_usage_percent !== undefined) {
            const avgUsage = Math.round((data.cpu.core0_usage_percent + data.cpu.core1_usage_percent) / 2);
            cpuEl.textContent = avgUsage + ' %';
            cpuEl.title = `Core0: ${data.cpu.core0_usage_percent}% | Core1: ${data.cpu.core1_usage_percent}%`;
            cpuEl.className = 'health-value';
            cpuEl.classList.add(`status-${data.cpu.status}`);
        } else {
            cpuEl.textContent = '사용 불가';
            cpuEl.className = 'health-value status-unavailable';
        }

    } catch (error) {
        console.error('Health 업데이트 실패:', error);
    }
}

// 업타임 포맷팅 (초 -> 일시분초)
function formatUptime(seconds) {
    const days = Math.floor(seconds / 86400);
    const hours = Math.floor((seconds % 86400) / 3600);
    const minutes = Math.floor((seconds % 3600) / 60);
    const secs = Math.floor(seconds % 60);

    if (days > 0) {
        return `${days}일 ${hours}시간`;
    } else if (hours > 0) {
        return `${hours}시간 ${minutes}분`;
    } else if (minutes > 0) {
        return `${minutes}분 ${secs}초`;
    } else {
        return `${secs}초`;
    }
}

// 비주얼 카메라 매핑 렌더링
function renderCameraMapping() {
    const container = document.getElementById('camera-mapping-buttons');
    container.innerHTML = '';

    // API 데이터 로드 전
    if (!switcherData) {
        container.innerHTML = '<p style="color: #6b7280;">Loading...</p>';
        return;
    }

    // effective 값 사용 (사용자 설정값과 스위처 실제값 중 작은 값)
    const primary = switcherData.primary.effective || 0;
    const secondary = switcherData.secondary.effective || 0;

    if (primary === 0 && secondary === 0) {
        container.innerHTML = '<p style="color: #6b7280;">No switcher configured.</p>';
        return;
    }

    // Secondary 시작 위치는 전역 변수 사용 (클릭으로 변경 가능)
    // 초기 로드 시에만 offset 값으로 설정
    // secondaryStartPos는 handleCameraClick에서 변경됨

    // total 계산: 항상 20채널 표시
    const total = 20;

    // 검증: Primary + Secondary + Other = 20 (Secondary가 Primary를 덮어씀)
    let primaryCount = 0, secondaryCount = 0, otherCount = 0;
    for (let i = 1; i <= total; i++) {
        // Secondary 영역
        if (secondary > 0 && i >= secondaryStartPos && i < secondaryStartPos + secondary) {
            secondaryCount++;
        }
        // Primary 영역 (Secondary와 겹치지 않는 부분만)
        else if (i <= primary && !(secondary > 0 && i >= secondaryStartPos && i < secondaryStartPos + secondary)) {
            primaryCount++;
        }
        // 나머지
        else {
            otherCount++;
        }
    }
    console.log(`[Mapping] Primary:${primaryCount} + Secondary:${secondaryCount} + Other:${otherCount} = ${primaryCount + secondaryCount + otherCount}`);

    // 버튼 생성
    for (let i = 1; i <= total; i++) {
        const btn = document.createElement('button');
        btn.className = 'camera-btn';
        btn.textContent = i;
        btn.type = 'button';

        // Combined Tally 기준으로 effective 영역 표시
        // Secondary가 있으면 offset 위치부터 Secondary로 덮어씀
        const secondaryEnd = secondaryStartPos + secondary;

        if (secondary > 0 && i >= secondaryStartPos && i < secondaryEnd) {
            // Secondary 영역 (파란색)
            btn.classList.add('secondary');
        }
        else if (i <= primary && (secondary === 0 || i < secondaryStartPos)) {
            // Primary 영역 (Secondary 시작 위치 이전만 표시)
            // Secondary가 Primary 영역으로 들어오면 Secondary 시작 위치 이후의 Primary는 제거
            btn.classList.add('primary');
        }
        else if (secondary > 0) {
            // Secondary가 있을 때: effective 범위 밖의 선택 가능 영역
            btn.classList.add('selectable-secondary');
        }

        // Secondary 시작 위치 버튼 강조 표시
        if (secondary > 0 && i === secondaryStartPos) {
            btn.classList.add('selected');
        }

        // 모든 버튼 클릭 가능
        if (secondary > 0) {
            btn.classList.add('clickable');
            btn.addEventListener('click', () => handleCameraClick(i));
        }

        container.appendChild(btn);
    }

    // Secondary 시작 위치 표시
    document.getElementById('secondary-start-pos').textContent =
        secondary > 0 ? secondaryStartPos : '-';
}

// 카메라 버튼 클릭 핸들러
function handleCameraClick(position) {
    secondaryStartPos = position;
    renderCameraMapping();
}

// 매핑 저장
async function handleMappingSave() {
    try {
        // Secondary 오프셋 저장 (offset = position - 1)
        const data = {
            index: 1,
            camera_limit: switcherData.secondary.limit,
            camera_offset: secondaryStartPos - 1
        };

        const response = await fetch(API.SWITCHER_MAPPING, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(data)
        });

        if (!response.ok) throw new Error('매핑 저장 실패');

        showMessage('카메라 매핑이 저장되었습니다.', 'success');

        // 설정 다시 로드
        setTimeout(() => {
            loadSwitcherConfig();
        }, 500);

    } catch (error) {
        console.error('매핑 저장 오류:', error);
        showMessage('매핑 저장에 실패했습니다.', 'error');
    }
}

// 토스트 메시지 표시
function showMessage(text, type = 'success') {
    const container = document.getElementById('toast-container');

    const toast = document.createElement('div');
    toast.className = `toast ${type}`;
    toast.textContent = text;

    container.appendChild(toast);

    // 3초 후 제거
    setTimeout(() => {
        toast.classList.add('hiding');
        setTimeout(() => {
            toast.remove();
        }, 300); // 애니메이션 시간
    }, 3000);
}

// ============================================================================
// 모달 제어
// ============================================================================

/**
 * 스위처 설정 모달 열기
 * @param {number} index - 스위처 인덱스 (0: Primary, 1: Secondary)
 */
function openSwitcherModal(index) {
    const modal = document.getElementById('switcher-modal');
    const title = document.getElementById('modal-title');
    const modalIndex = document.getElementById('modal-index');
    const typeSelect = document.getElementById('modal-type');

    // 모달 제목 설정
    title.textContent = index === 0 ? 'Primary Switcher 설정' : 'Secondary Switcher 설정';
    modalIndex.value = index;

    // 현재 설정 불러오기
    if (switcherData && switcherData.switchers && switcherData.switchers[index]) {
        const config = switcherData.switchers[index];
        document.getElementById('modal-type').value = config.type;
        document.getElementById('modal-interface').value = config.interface;
        document.getElementById('modal-ip').value = config.ip;
        document.getElementById('modal-password').value = config.password || '';
        document.getElementById('modal-camera-limit').value = config.camera_limit || 0;
    } else {
        // 기본값 설정
        document.getElementById('modal-type').value = '1';  // ATEM
        document.getElementById('modal-interface').value = '2';  // Ethernet
        document.getElementById('modal-ip').value = '';
        document.getElementById('modal-password').value = '';
        document.getElementById('modal-camera-limit').value = 0;
    }

    // 타입 변경 이벤트 리스너 등록
    typeSelect.onchange = updateModalFields;

    // 초기 필드 상태 설정
    updateModalFields();

    // 모달 표시
    modal.classList.add('show');

    // 이벤트 리스너 등록 (중복 방지를 위해 기존 리스너 제거 후 추가)
    const saveBtn = document.getElementById('modal-save-btn');
    saveBtn.onclick = handleModalSave;
}

/**
 * 스위처 타입에 따라 모달 필드 표시/숨김
 */
function updateModalFields() {
    const type = parseInt(document.getElementById('modal-type').value);
    const passwordGroup = document.getElementById('modal-password-group');

    // OBS(3)인 경우만 비밀번호 필드 표시
    if (type === 3) {
        passwordGroup.style.display = 'block';
    } else {
        passwordGroup.style.display = 'none';
        document.getElementById('modal-password').value = '';  // 값 초기화
    }
}

/**
 * 스위처 설정 모달 닫기
 */
function closeSwitcherModal() {
    const modal = document.getElementById('switcher-modal');
    modal.classList.remove('show');
}

/**
 * 모달 저장 버튼 핸들러
 */
async function handleModalSave() {
    const index = parseInt(document.getElementById('modal-index').value);
    const type = parseInt(document.getElementById('modal-type').value);
    const interfaceType = parseInt(document.getElementById('modal-interface').value);
    const ip = document.getElementById('modal-ip').value;
    const password = document.getElementById('modal-password').value;
    const cameraLimit = parseInt(document.getElementById('modal-camera-limit').value) || 0;

    // 유효성 검사
    if (!ip) {
        showMessage('IP 주소를 입력해주세요.', 'error');
        return;
    }

    try {
        const response = await fetch(API.SWITCHER, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                index: index,
                type: type,
                interface: interfaceType,
                ip: ip,
                password: password,
                camera_limit: cameraLimit
            })
        });

        if (!response.ok) throw new Error('저장 실패');

        showMessage(`스위처 ${index === 0 ? 'Primary' : 'Secondary'} 설정이 저장되었습니다. 스위처 연결 재시작이 필요합니다.`, 'success');

        // 모달 닫기
        closeSwitcherModal();

        // 설정 새로고침
        setTimeout(() => {
            loadSwitcherConfig();
        }, 500);

    } catch (error) {
        console.error('저장 오류:', error);
        showMessage('설정 저장에 실패했습니다.', 'error');
    }
}

// 모달 외부 클릭 시 닫기
window.addEventListener('click', (event) => {
    const modal = document.getElementById('switcher-modal');
    if (event.target === modal) {
        closeSwitcherModal();
    }
});

// ============================================================================
// LoRa 관련 함수
// ============================================================================

/**
 * LoRa 상태 업데이트
 */
/**
 * LoRa 상태 업데이트 (데이터로부터)
 */
function updateLoRaStatusFromData(status) {
    // 칩 정보
    const chipBadge = document.getElementById('lora-chip');
    chipBadge.textContent = status.chip_name || 'Unknown';
    chipBadge.className = status.initialized ? 'badge badge-success' : 'badge badge-error';

    // 주파수
    document.getElementById('lora-frequency').textContent =
        status.frequency ? `${status.frequency.toFixed(1)} MHz` : '-';

    // API에서 받은 주파수 범위 사용
    const minFreq = status.freq_min || 850;
    const maxFreq = status.freq_max || 930;
    const rangeText = `${minFreq.toFixed(0)}~${maxFreq.toFixed(0)} MHz (${status.chip_name})`;

    // 스캔 범위 표시
    document.getElementById('scan-range-display').textContent = rangeText;

    // 주파수 입력 필드 설정
    const freqInput = document.getElementById('lora-freq-input');
    freqInput.min = minFreq;
    freqInput.max = maxFreq;
    // 처음 한 번만 값 설정 (이후 업데이트 안 함)
    if (!freqInput.dataset.initialized) {
        freqInput.value = status.frequency || ((minFreq + maxFreq) / 2);
        freqInput.dataset.initialized = 'true';
    }

    // 주파수 범위 힌트
    document.getElementById('freq-range-hint').textContent = `지원 범위: ${rangeText}`;

    // Sync Word 슬라이더 설정
    const syncInput = document.getElementById('lora-sync-input');
    const syncValue = document.getElementById('lora-sync-value');
    // 처음 한 번만 값 설정 (이후 업데이트 안 함)
    if (!syncInput.dataset.initialized) {
        const syncWordValue = status.sync_word !== undefined ? status.sync_word : 0x12;
        syncInput.value = syncWordValue;
        syncValue.textContent = '0x' + syncWordValue.toString(16).toUpperCase().padStart(2, '0');
        syncInput.dataset.initialized = 'true';
    }

    // 전역 변수에 저장 (스캔에서 사용)
    window.loraFreqRange = { min: minFreq, max: maxFreq };
}

/**
 * LoRa 상태 업데이트 (API 호출) - 초기 로드용
 */
async function updateLoRaStatus() {
    try {
        const response = await fetch(API.STATUS);
        if (!response.ok) throw new Error('상태 조회 실패');

        const data = await response.json();
        if (data.lora) {
            updateLoRaStatusFromData(data.lora);
        }

    } catch (error) {
        console.error('LoRa 상태 업데이트 오류:', error);
    }
}

/**
 * LoRa 주파수 스캔
 */
async function handleLoRaScan() {
    // 주파수 범위는 칩 타입에 따라 자동 설정
    if (!window.loraFreqRange) {
        showMessage('LoRa 상태를 먼저 확인해주세요.', 'error');
        return;
    }

    const startFreq = window.loraFreqRange.min;
    const endFreq = window.loraFreqRange.max;
    const step = 1.0;  // 고정값: 1MHz

    const channelCount = Math.floor((endFreq - startFreq) / step) + 1;
    const estimatedTime = (channelCount * 0.1).toFixed(1);

    const scanBtn = document.getElementById('lora-scan-btn');
    const scanStatus = document.getElementById('lora-scan-status');

    scanBtn.disabled = true;
    scanStatus.textContent = `스캔 중... (약 ${estimatedTime}초 소요, ${channelCount}개 채널)`;

    try {
        const response = await fetch(`${API.LORA_SCAN}?start=${startFreq}&end=${endFreq}`);
        if (!response.ok) throw new Error('스캔 실패');

        const result = await response.json();

        // 결과 표시
        displayScanResults(result.channels);
        scanStatus.textContent = `완료: ${result.count}개 채널 스캔됨`;
        showMessage(`주파수 스캔 완료: ${result.count}개 채널`, 'success');

    } catch (error) {
        console.error('스캔 오류:', error);
        const errorMsg = error.message || '알 수 없는 오류';
        scanStatus.textContent = `스캔 실패: ${errorMsg}`;
        showMessage(`주파수 스캔 실패: ${errorMsg}`, 'error');
    } finally {
        scanBtn.disabled = false;
    }
}

/**
 * 스캔 결과 시각화
 */
function displayScanResults(channels) {
    const resultsDiv = document.getElementById('lora-scan-results');
    const chartDiv = document.getElementById('lora-scan-chart');

    if (!channels || channels.length === 0) {
        resultsDiv.style.display = 'none';
        return;
    }

    resultsDiv.style.display = 'block';

    // 차트 생성 (간단한 바 차트) - 10개만 보이고 스크롤
    let html = '<div style="display: flex; flex-direction: column; gap: 5px; max-height: 300px; overflow-y: auto;">';

    channels.forEach(ch => {
        const rssi = ch.rssi;

        // RSSI를 0~100 스케일로 변환 (-100 dBm = 100%, -50 dBm = 0%, -100 이하는 100%)
        const rssiPercent = Math.max(0, Math.min(100, ((-50 - rssi) / 50) * 100));

        // -80 dBm 이하 = 녹색, 이상 = 빨간색
        const color = (rssi < -80) ? '#4CAF50' : '#f44336';

        html += `
            <div style="display: flex; align-items: center; gap: 10px;">
                <div style="width: 80px; text-align: right; font-size: 12px;">${ch.frequency.toFixed(1)} MHz</div>
                <div style="flex: 1; background: #eee; height: 20px; border-radius: 3px; overflow: visible; position: relative; cursor: pointer;"
                     onclick="document.getElementById('lora-freq-input').value = ${ch.frequency.toFixed(1)};">
                    <div style="width: ${rssiPercent}%; height: 100%; background: ${color};"></div>
                    <div style="position: absolute; right: 5px; top: 50%; transform: translateY(-50%); font-size: 10px; color: #666; pointer-events: none;">
                        ${rssi.toFixed(1)}
                    </div>
                </div>
            </div>
        `;
    });

    html += '</div>';
    chartDiv.innerHTML = html;
}

/**
 * LoRa 주파수 변경
 */
async function handleLoRaFrequencyChange() {
    const frequency = parseFloat(document.getElementById('lora-freq-input').value);
    const syncWord = parseInt(document.getElementById('lora-sync-input').value);

    if (isNaN(frequency)) {
        showMessage('올바른 주파수를 입력해주세요.', 'error');
        return;
    }

    // 칩 타입에 따른 범위 체크
    if (window.loraFreqRange) {
        if (frequency < window.loraFreqRange.min || frequency > window.loraFreqRange.max) {
            showMessage(`주파수는 ${window.loraFreqRange.min}~${window.loraFreqRange.max} MHz 범위여야 합니다.`, 'error');
            return;
        }
    }

    try {
        const response = await fetch(API.LORA_CONFIG, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ frequency: frequency, sync_word: syncWord })
        });

        if (!response.ok) throw new Error('주파수 임시 저장 실패');

        // 대기 중 표시
        document.getElementById('freq-pending-badge').style.display = 'inline';
        showMessage(`주파수 ${frequency.toFixed(1)} MHz 임시 저장됨 (아래 "설정 적용하기" 버튼 클릭 필요)`, 'success');

    } catch (error) {
        console.error('주파수 임시 저장 오류:', error);
        showMessage('주파수 임시 저장에 실패했습니다.', 'error');
    }
}

/**
 * LoRa Sync Word 변경
 */
async function handleLoRaSyncWordChange() {
    const frequency = parseFloat(document.getElementById('lora-freq-input').value);
    const syncWord = parseInt(document.getElementById('lora-sync-input').value);

    try {
        const response = await fetch(API.LORA_CONFIG, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ frequency: frequency, sync_word: syncWord })
        });

        if (!response.ok) throw new Error('Sync Word 임시 저장 실패');

        // 대기 중 표시
        document.getElementById('sync-pending-badge').style.display = 'inline';
        showMessage(`Sync Word 0x${syncWord.toString(16).toUpperCase()} 임시 저장됨 (아래 "설정 적용하기" 버튼 클릭 필요)`, 'success');

    } catch (error) {
        console.error('Sync Word 임시 저장 오류:', error);
        showMessage('Sync Word 임시 저장에 실패했습니다.', 'error');
    }
}

/**
 * LoRa 설정 적용 (3회 송신 + 1초 대기 + TX 변경)
 */
async function handleLoRaApply() {
    const applyBtn = document.getElementById('lora-apply-btn');
    const statusDiv = document.getElementById('lora-apply-status');

    applyBtn.disabled = true;
    statusDiv.style.color = '#0066cc';
    statusDiv.innerHTML = '📡 RX 장치에 알림 전송 중... (1/3)';

    try {
        // 실제 API 호출 (백엔드에서 전체 프로세스 수행)
        const response = await fetch('/api/lora/apply', {
            method: 'POST'
        });

        if (!response.ok) throw new Error('설정 적용 실패');

        // 진행 상황 표시 (백엔드에서 처리 중)
        await new Promise(r => setTimeout(r, 1000));
        statusDiv.innerHTML = '📡 RX 장치에 알림 전송 중... (2/3)';

        await new Promise(r => setTimeout(r, 1000));
        statusDiv.innerHTML = '📡 RX 장치에 알림 전송 중... (3/3)';

        await new Promise(r => setTimeout(r, 1000));
        statusDiv.innerHTML = '⏳ RX 장치 설정 변경 대기 중...';

        await new Promise(r => setTimeout(r, 1000));
        statusDiv.innerHTML = '🔄 TX 설정 변경 중...';

        await new Promise(r => setTimeout(r, 500));

        // 성공
        statusDiv.style.color = '#28a745';
        statusDiv.innerHTML = '✅ 설정 적용 완료!';

        // 대기 중 표시 제거
        document.getElementById('freq-pending-badge').style.display = 'none';
        document.getElementById('sync-pending-badge').style.display = 'none';

        showMessage('LoRa 설정이 적용되었습니다', 'success');

        // 상태 새로고침
        setTimeout(() => {
            updateStatus();
            statusDiv.innerHTML = '';
        }, 2000);

    } catch (error) {
        statusDiv.style.color = '#dc3545';
        statusDiv.innerHTML = '❌ 설정 적용 실패';
        showMessage('설정 적용에 실패했습니다', 'error');
        console.error('설정 적용 오류:', error);
    } finally {
        applyBtn.disabled = false;
    }
}
