// Tally Node Web Application
(() => {
  // src/js/modules/state.js
  function stateModule() {
    return {
      // 초기화 완료 플래그
      _initialized: false,
      // 폴링 타이머
      _statusPollingTimer: null,
      // 현재 뷰
      currentView: "dashboard",
      // 뷰 타이틀 매핑
      viewTitles: {
        dashboard: "Dashboard",
        network: "Network",
        switcher: "Switcher",
        broadcast: "Broadcast",
        devices: "Devices",
        system: "System",
        license: "License Management"
      },
      // WebSocket 연결 상태
      wsConnected: false,
      // /api/status 응답 데이터 (캐시)
      status: {
        network: { wifi: { connected: false, ssid: "", ip: "--" }, ethernet: { connected: false, ip: "--" } },
        switcher: {
          dualEnabled: false,
          secondaryOffset: 4,
          primary: { connected: false, type: "ATEM", ip: "", port: 0, tally: { pgm: [], pvw: [], raw: "", channels: 0 } },
          secondary: { connected: false, type: "ATEM", ip: "", port: 0, tally: { pgm: [], pvw: [], raw: "", channels: 0 } }
        },
        system: { deviceId: "0000", battery: 0, voltage: 0, temperature: 0, uptime: 0 }
      },
      // 네트워크 상태 (UI용 별칭)
      network: {
        wifiConnected: false,
        wifiSsid: "",
        wifiIp: "--",
        ethConnected: false,
        ethDetected: false,
        ethIp: "--",
        apEnabled: false,
        apSsid: "",
        apChannel: 1,
        apIp: "--"
      },
      // 시스템 정보
      system: {
        deviceId: "0000",
        battery: 0,
        voltage: 0,
        temperature: 0,
        uptime: 0,
        freeHeap: 0,
        version: "0.1.0"
      },
      // 설정 데이터
      config: {
        network: {
          wifiAp: { ssid: "", channel: 1, enabled: false },
          wifiSta: { ssid: "", enabled: false },
          ethernet: { dhcp: true, staticIp: "", netmask: "", gateway: "", enabled: false }
        },
        switcher: {
          primary: { connected: false, type: "ATEM", ip: "", port: 0, interface: 2, cameraLimit: 0, tally: { pgm: [], pvw: [], raw: "", channels: 0 } },
          secondary: { connected: false, type: "ATEM", ip: "", port: 0, interface: 1, cameraLimit: 0, tally: { pgm: [], pvw: [], raw: "", channels: 0 } },
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
        }
      },
      // 폼 입력 임시 데이터
      form: {
        ap: { ssid: "", password: "", channel: 1, enabled: false },
        wifi: { ssid: "", password: "", enabled: false },
        ethernet: { dhcp: true, ip: "", gateway: "", netmask: "", enabled: false },
        switcher: {
          primary: { type: "ATEM", ip: "", port: 9910, interface: 0, cameraLimit: 0, password: "", portLocked: true },
          secondary: { type: "ATEM", ip: "", port: 9910, interface: 0, cameraLimit: 0, password: "", portLocked: true },
          dualEnabled: false,
          secondaryOffset: 4
        },
        mappingOffset: 4,
        display: { brightness: 128, cameraId: 1 },
        broadcast: { syncCode: 18, frequency: 868 }
      },
      /**
       * 뷰 타이틀 계산
       */
      viewTitle() {
        return this.viewTitles[this.currentView] || "TALLY-NODE";
      },
      /**
       * 초기화
       */
      async init() {
        console.log("Tally App initializing...");
        const hash = window.location.hash.slice(1);
        if (hash && ["dashboard", "network", "switcher", "broadcast", "devices", "system", "license"].includes(hash)) {
          this.currentView = hash;
        }
        window.addEventListener("hashchange", () => {
          const newHash = window.location.hash.slice(1);
          if (newHash && ["dashboard", "network", "switcher", "broadcast", "devices", "system", "license"].includes(newHash)) {
            this.currentView = newHash;
          }
        });
        await this.fetchStatus();
        await this.initDevices();
        await this.initLicense();
        this.startStatusPolling();
      },
      /**
       * 상태 조회
       */
      async fetchStatus() {
        try {
          const res = await fetch("/api/status");
          if (!res.ok)
            throw new Error("Status fetch failed");
          const data = await res.json();
          this.status = { ...this.status, ...data };
          if (data.network) {
            if (data.network.ap) {
              this.network.apEnabled = data.network.ap.enabled || false;
              this.network.apSsid = data.network.ap.ssid || "";
              this.network.apChannel = data.network.ap.channel || 1;
              this.network.apIp = data.network.ap.ip || "--";
              if (!this._initialized) {
                this.form.ap = {
                  enabled: data.network.ap.enabled,
                  ssid: data.network.ap.ssid || "",
                  channel: data.network.ap.channel || 1,
                  password: this.form.ap.password
                };
              }
            }
            if (data.network.wifi) {
              this.network.wifiConnected = data.network.wifi.connected;
              this.network.wifiSsid = data.network.wifi.ssid || "";
              this.network.wifiIp = data.network.wifi.ip || "--";
              if (!this._initialized) {
                this.form.wifi.enabled = data.network.wifi.enabled;
                this.form.wifi.ssid = data.network.wifi.ssid || "";
              }
            }
            if (data.network.ethernet) {
              this.network.ethConnected = data.network.ethernet.connected;
              this.network.ethDetected = data.network.ethernet.detected || false;
              this.network.ethIp = data.network.ethernet.ip || "--";
              if (!this._initialized) {
                this.form.ethernet.enabled = data.network.ethernet.enabled;
                this.form.ethernet.dhcp = data.network.ethernet.dhcp;
                this.form.ethernet.ip = data.network.ethernet.staticIp || "";
                this.form.ethernet.netmask = data.network.ethernet.netmask || "";
                this.form.ethernet.gateway = data.network.ethernet.gateway || "";
              }
            }
          }
          if (data.system) {
            this.system.deviceId = data.system.deviceId || "0000";
            this.system.battery = data.system.battery || 0;
            this.system.voltage = data.system.voltage || 0;
            this.system.temperature = data.system.temperature || 0;
            this.system.uptime = data.system.uptime || 0;
          }
          if (data.switcher) {
            this.config.switcher.dualEnabled = data.switcher.dualEnabled || false;
            this.config.switcher.secondaryOffset = data.switcher.secondaryOffset || 4;
            if (!this._initialized) {
              this.form.switcher.dualEnabled = data.switcher.dualEnabled || false;
              this.form.switcher.secondaryOffset = data.switcher.secondaryOffset || 4;
              this.form.mappingOffset = (data.switcher.secondaryOffset || 4) + 1;
            }
            if (data.switcher.primary) {
              const primaryData = {
                connected: data.switcher.primary.connected || false,
                type: data.switcher.primary.type || "ATEM",
                ip: data.switcher.primary.ip || "",
                port: data.switcher.primary.port || 0,
                interface: data.switcher.primary.interface || 0,
                cameraLimit: data.switcher.primary.cameraLimit || 0,
                tally: data.switcher.primary.tally || { pgm: [], pvw: [], raw: "", channels: 0 }
              };
              this.config.switcher.primary = primaryData;
              if (!this.status.switcher)
                this.status.switcher = { primary: {}, secondary: {} };
              this.status.switcher.primary = { ...primaryData };
              if (!this._initialized) {
                this.form.switcher.primary.type = data.switcher.primary.type || "ATEM";
                this.form.switcher.primary.ip = data.switcher.primary.ip || "";
                this.form.switcher.primary.port = data.switcher.primary.port || 9910;
                this.form.switcher.primary.interface = data.switcher.primary.interface || 0;
                this.form.switcher.primary.cameraLimit = data.switcher.primary.cameraLimit || 0;
                this.form.switcher.primary.password = data.switcher.primary.password || "";
                this.form.switcher.primary.portLocked = true;
              }
            }
            if (data.switcher.secondary) {
              const secondaryData = {
                connected: data.switcher.secondary.connected || false,
                type: data.switcher.secondary.type || "ATEM",
                ip: data.switcher.secondary.ip || "",
                port: data.switcher.secondary.port || 0,
                interface: data.switcher.secondary.interface || 0,
                cameraLimit: data.switcher.secondary.cameraLimit || 0,
                tally: data.switcher.secondary.tally || { pgm: [], pvw: [], raw: "", channels: 0 }
              };
              this.config.switcher.secondary = secondaryData;
              if (!this.status.switcher)
                this.status.switcher = { primary: {}, secondary: {} };
              this.status.switcher.secondary = { ...secondaryData };
              if (!this._initialized) {
                this.form.switcher.secondary.type = data.switcher.secondary.type || "ATEM";
                this.form.switcher.secondary.ip = data.switcher.secondary.ip || "";
                this.form.switcher.secondary.port = data.switcher.secondary.port || 9910;
                this.form.switcher.secondary.interface = data.switcher.secondary.interface || 0;
                this.form.switcher.secondary.cameraLimit = data.switcher.secondary.cameraLimit || 0;
                this.form.switcher.secondary.password = data.switcher.secondary.password || "";
                this.form.switcher.secondary.portLocked = true;
              }
            }
          }
          if (data.broadcast && data.broadcast.rf) {
            this.config.broadcast.rf = {
              frequency: data.broadcast.rf.frequency || 868,
              syncWord: data.broadcast.rf.syncWord || 18,
              spreadingFactor: data.broadcast.rf.spreadingFactor || 7,
              codingRate: data.broadcast.rf.codingRate || 7,
              bandwidth: data.broadcast.rf.bandwidth || 250,
              txPower: data.broadcast.rf.txPower || 22
            };
            if (!this._initialized) {
              this.form.broadcast.syncCode = data.broadcast.rf.syncWord || 18;
              this.form.broadcast.frequency = data.broadcast.rf.frequency || 868;
            }
          } else {
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
              this.form.broadcast.frequency = 868;
            }
          }
          this._initialized = true;
          this.wsConnected = true;
        } catch (e) {
          console.error("Status fetch error:", e);
          this.wsConnected = false;
        }
      },
      /**
       * 상태 폴링 시작 (스위처 카드 정보 주기 업데이트)
       */
      startStatusPolling() {
        if (this._statusPollingTimer)
          return;
        this._statusPollingTimer = setInterval(async () => {
          await this.fetchStatus();
        }, 2e3);
      },
      /**
       * 상태 폴링 중지
       */
      stopStatusPolling() {
        if (this._statusPollingTimer) {
          clearInterval(this._statusPollingTimer);
          this._statusPollingTimer = null;
        }
      }
    };
  }

  // src/js/modules/network.js
  function networkModule() {
    return {
      /**
       * AP 설정 저장
       */
      async saveAp() {
        try {
          const res = await fetch("/api/config/network/ap", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({
              ssid: this.form.ap.ssid,
              password: this.form.ap.password,
              channel: this.form.ap.channel,
              enabled: this.form.ap.enabled
            })
          });
          const data = await res.json();
          if (data.status === "ok") {
            this.showToast("WiFi AP settings saved - Reconnect if IP changed", "alert-success");
            await this.fetchStatus();
          } else {
            this.showToast(data.message || "Failed to save WiFi AP settings", "alert-error");
          }
        } catch (e) {
          this.showToast("Network error. Please try again.", "alert-error");
        }
      },
      /**
       * WiFi STA 설정 저장
       */
      async saveWifi() {
        try {
          const res = await fetch("/api/config/network/wifi", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({
              ssid: this.form.wifi.ssid,
              password: this.form.wifi.password,
              enabled: this.form.wifi.enabled
            })
          });
          const data = await res.json();
          if (data.status === "ok") {
            this.showToast("WiFi STA settings saved - Reconnect if IP changed", "alert-success");
            await this.fetchStatus();
          } else {
            this.showToast(data.message || "Failed to save WiFi STA settings", "alert-error");
          }
        } catch (e) {
          this.showToast("Network error. Please try again.", "alert-error");
        }
      },
      /**
       * Ethernet 설정 저장
       */
      async saveEthernet() {
        try {
          const res = await fetch("/api/config/network/ethernet", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({
              dhcp: this.form.ethernet.dhcp,
              staticIp: this.form.ethernet.ip,
              gateway: this.form.ethernet.gateway,
              netmask: this.form.ethernet.netmask,
              enabled: this.form.ethernet.enabled
            })
          });
          const data = await res.json();
          if (data.status === "ok") {
            this.showToast("Ethernet settings saved - Reconnect if IP changed", "alert-success");
            await this.fetchStatus();
          } else {
            this.showToast(data.message || "Failed to save Ethernet settings", "alert-error");
          }
        } catch (e) {
          this.showToast("Network error. Please try again.", "alert-error");
        }
      }
    };
  }

  // src/js/modules/switcher.js
  function switcherModule() {
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
          if (this.form.switcher.primary.type === "ATEM") {
            payload.interface = this.form.switcher.primary.interface;
          }
          if (this.form.switcher.primary.type === "OBS") {
            payload.password = this.form.switcher.primary.password || "";
          }
          const res = await fetch("/api/config/switcher/primary", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify(payload)
          });
          const data = await res.json();
          if (data.status === "ok") {
            this.showToast(`Primary switcher saved: ${this.form.switcher.primary.type} @ ${this.form.switcher.primary.ip || "--"}`, "alert-success");
            this.showPrimaryConfig = false;
            await this.fetchStatus();
          } else {
            this.showToast(data.message || "Failed to save primary switcher", "alert-error");
          }
        } catch (e) {
          this.showToast("Network error. Please try again.", "alert-error");
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
          if (this.form.switcher.secondary.type === "ATEM") {
            payload.interface = this.form.switcher.secondary.interface;
          }
          if (this.form.switcher.secondary.type === "OBS") {
            payload.password = this.form.switcher.secondary.password || "";
          }
          const res = await fetch("/api/config/switcher/secondary", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify(payload)
          });
          const data = await res.json();
          if (data.status === "ok") {
            this.showToast(`Secondary switcher saved: ${this.form.switcher.secondary.type} @ ${this.form.switcher.secondary.ip || "--"}`, "alert-success");
            this.showSecondaryConfig = false;
            await this.fetchStatus();
          } else {
            this.showToast(data.message || "Failed to save secondary switcher", "alert-error");
          }
        } catch (e) {
          this.showToast("Network error. Please try again.", "alert-error");
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
          ATEM: "ATEM",
          vMix: "vMix",
          OBS: "OBS (In Development)",
          OSEE: "OSEE (In Development)"
        };
        return names[type] || type;
      },
      /**
       * 스위처 타입별 설명 반환
       */
      getSwitcherTypeDesc(type) {
        const descs = {
          ATEM: "Blackmagic Design ATEM Switcher",
          vMix: "vMix Live Video Production Software",
          OBS: "OBS Studio (In Development)",
          OSEE: "OSEE (In Development)"
        };
        return descs[type] || "";
      },
      /**
       * 인터페이스 값에 따른 이름 반환
       */
      getInterfaceName(val) {
        const names = { 0: "Auto", 1: "Ethernet", 2: "WiFi" };
        return names[val] || "Auto";
      },
      /**
       * 스위처 타입 변경 시 처리 (포트 자동 변경)
       */
      onSwitcherTypeChange(role) {
        const sw = this.form.switcher[role];
        sw.port = this.getDefaultPort(sw.type);
        if (sw.type !== "ATEM") {
          sw.interface = 0;
        }
      },
      /**
       * 듀얼 모드 변경 시 처리
       */
      async onDualModeChange() {
        if (this.form.switcher.dualEnabled) {
          this.form.mappingOffset = this.config.switcher.secondaryOffset + 1;
        }
        try {
          const res = await fetch("/api/config/switcher/dual", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({
              dualEnabled: this.form.switcher.dualEnabled
            })
          });
          const data = await res.json();
          if (data.status === "ok") {
            this.showToast(
              this.form.switcher.dualEnabled ? "Dual mode enabled - Configure secondary start position" : "Dual mode disabled",
              "alert-success"
            );
            await this.fetchStatus();
          } else {
            this.showToast(data.message || "Failed to save dual mode", "alert-error");
            this.form.switcher.dualEnabled = !this.form.switcher.dualEnabled;
          }
        } catch (e) {
          this.showToast("Network error. Please try again.", "alert-error");
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
        const primaryChannels = primary?.connected ? primary.tally?.channels || 0 : 0;
        const secondaryChannels = secondary?.connected && dualEnabled ? secondary.tally?.channels || 0 : 0;
        if (!dualEnabled) {
          if (cameraNum <= primaryChannels) {
            return "bg-blue-500 text-white cursor-pointer hover:bg-blue-600";
          }
          return "bg-slate-100 text-slate-400 cursor-pointer hover:bg-slate-200";
        }
        const primaryLimited = Math.min(primaryChannels, offset - 1);
        const secondaryEnd = Math.min(offset + secondaryChannels - 1, 20);
        if (cameraNum <= primaryLimited) {
          return "bg-blue-500 text-white cursor-pointer hover:bg-blue-600";
        }
        if (cameraNum < offset) {
          return "bg-slate-100 text-slate-400 cursor-pointer hover:bg-slate-200";
        }
        if (cameraNum === offset) {
          return secondaryChannels > 0 ? "bg-purple-700 text-white ring-2 ring-purple-400 ring-offset-2 cursor-pointer hover:bg-purple-800" : "bg-slate-300 text-slate-600 cursor-pointer hover:bg-slate-400";
        }
        if (cameraNum <= secondaryEnd) {
          return "bg-purple-500 text-white cursor-pointer hover:bg-purple-600";
        }
        return "bg-slate-100 text-slate-400 cursor-pointer hover:bg-slate-200";
      },
      /**
       * 카메라 버튼 라벨 생성
       */
      getCameraLabel(cameraNum) {
        const offset = this.form.mappingOffset;
        const primary = this.status.switcher?.primary;
        const secondary = this.status.switcher?.secondary;
        const dualEnabled = this.form.switcher.dualEnabled;
        const primaryChannels = primary?.connected ? primary.tally?.channels || 0 : 0;
        const secondaryChannels = secondary?.connected && dualEnabled ? secondary.tally?.channels || 0 : 0;
        if (!dualEnabled) {
          if (cameraNum <= primaryChannels) {
            return `${cameraNum}(P${cameraNum})`;
          }
          return `${cameraNum}`;
        }
        const primaryLimited = Math.min(primaryChannels, offset - 1);
        const secondaryStart = offset;
        if (cameraNum <= primaryLimited) {
          return `${cameraNum}(P${cameraNum})`;
        }
        if (cameraNum >= secondaryStart) {
          const secondaryIndex = cameraNum - secondaryStart + 1;
          if (secondaryIndex <= secondaryChannels) {
            return `${cameraNum}(S${secondaryIndex})`;
          }
        }
        return `${cameraNum}`;
      },
      /**
       * Offset 선택
       */
      selectOffset(cameraNum) {
        if (!this.form.switcher.dualEnabled)
          return;
        this.form.mappingOffset = cameraNum;
      },
      /**
       * 매핑 저장 (Secondary offset)
       */
      async saveMapping() {
        try {
          const res = await fetch("/api/config/switcher/dual", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({
              dualEnabled: this.form.switcher.dualEnabled,
              secondaryOffset: this.form.mappingOffset - 1
              // 0-based로 변환
            })
          });
          const data = await res.json();
          if (data.status === "ok") {
            this.showToast(`Mapping saved: Secondary starts at camera ${this.form.mappingOffset}`, "alert-success");
            await this.fetchStatus();
          } else {
            this.showToast(data.message || "Failed to save mapping", "alert-error");
          }
        } catch (e) {
          this.showToast("Network error. Please try again.", "alert-error");
        }
      }
    };
  }

  // src/js/modules/broadcast.js
  function broadcastModule() {
    return {
      // Broadcast 주파수 프리셋 (EoRa-S3-900TB: 850-930MHz)
      channelPresets: {
        frequencies: [850, 860, 868, 870, 880, 900, 915, 925, 930]
      },
      // 채널 스캔 상태
      channelScan: {
        scanning: false,
        progress: 0,
        startFreq: 850,
        endFreq: 930,
        step: 1,
        results: [],
        recommendation: null,
        currentRssi: 0,
        currentStatus: "unknown"
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
          const res = await fetch("/api/config/device/rf", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({
              frequency: Number(this.form.broadcast.frequency),
              syncWord: Number(this.form.broadcast.syncCode)
            })
          });
          const data = await res.json();
          if (data.status === "ok") {
            await new Promise((resolve) => setTimeout(resolve, 6e3));
            await this.fetchStatus();
            this.broadcast.saving = false;
            this.showToast(`Channel saved: ${Math.round(this.form.broadcast.frequency)} MHz, Sync 0x${Number(this.form.broadcast.syncCode).toString(16).toUpperCase().padStart(2, "0")}`, "alert-success");
          } else {
            this.broadcast.saving = false;
            this.showToast(data.message || "Failed to save channel settings", "alert-error");
          }
        } catch (e) {
          this.broadcast.saving = false;
          this.showToast("Network error. Please try again.", "alert-error");
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
          const res = await fetch("/api/lora/scan/start", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({
              startFreq: 850,
              endFreq: 930,
              step: 1
            })
          });
          const data = await res.json();
          if (data.status === "started") {
            this.showToast("Scanning 850-930 MHz...", "alert-info");
            this.startChannelScanPolling();
          } else {
            this.channelScan.scanning = false;
            this.showToast(data.message || "Failed to start scan", "alert-error");
          }
        } catch (e) {
          this.channelScan.scanning = false;
          this.showToast("Network error. Please try again.", "alert-error");
        }
      },
      /**
       * 채널 주파수 스캔 중지
       */
      async stopChannelScan() {
        try {
          await fetch("/api/lora/scan/stop", { method: "POST" });
          this.channelScan.scanning = false;
          this.stopChannelScanPolling();
          this.showToast("Scan cancelled", "alert-warning");
        } catch (e) {
          this.showToast("Failed to stop scan", "alert-error");
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
          const res = await fetch("/api/lora/scan");
          if (!res.ok)
            return;
          const data = await res.json();
          this.channelScan.scanning = data.scanning;
          this.channelScan.progress = data.progress || 0;
          if (data.results && data.results.length > 0) {
            this.channelScan.results = data.results.map((r) => {
              const gaugePercent = Math.max(0, Math.min(100, (-50 - r.rssi) / 50 * 100));
              return {
                frequency: r.frequency,
                rssi: r.rssi,
                noiseFloor: r.noiseFloor,
                clearChannel: r.clearChannel,
                gaugePercent: gaugePercent.toFixed(1)
              };
            });
            this.analyzeScanRecommendation();
          }
          if (!data.scanning && data.progress === 100) {
            this.stopChannelScanPolling();
            this.showToast(`Scan complete: ${data.results?.length || 0} channels found`, "alert-success");
          }
        } catch (e) {
          console.error("Channel scan status fetch error:", e);
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
        const currentResult = this.channelScan.results.find((r) => Math.abs(r.frequency - currentFreq) < 0.5);
        if (currentResult) {
          this.channelScan.currentRssi = currentResult.rssi;
          this.channelScan.currentStatus = currentResult.clearChannel ? "clear" : "busy";
        } else {
          this.channelScan.currentRssi = 0;
          this.channelScan.currentStatus = "unknown";
        }
        const sortedByRssi = [...this.channelScan.results].sort((a, b) => a.rssi - b.rssi);
        const quietest = sortedByRssi[0];
        const quietestRssi = quietest.rssi;
        if (currentResult) {
          if (currentResult.clearChannel) {
          } else {
            this.channelScan.recommendation = {
              type: "change",
              title: "Current channel is noisy",
              message: `Recommend switching to quieter channel`,
              suggestedFreq: quietest.frequency
            };
          }
        } else {
          this.channelScan.recommendation = {
            type: "better",
            title: "Frequency outside scan range",
            message: `Consider switching to ${quietest.frequency.toFixed(1)} MHz`,
            suggestedFreq: quietest.frequency
          };
        }
      },
      /**
       * RSSI 게이지 색상 계산
       */
      getGaugeColor(rssi) {
        if (rssi <= -90)
          return "#22c55e";
        if (rssi <= -80)
          return "#eab308";
        return "#ef4444";
      },
      /**
       * RSSI 게이지 stroke-dasharray 계산
       */
      getGaugeCircumference(rssi) {
        const r = 16;
        const circumference = 2 * Math.PI * r;
        const percent = Math.max(0, Math.min(100, (-50 - rssi) / 50 * 100));
        const filled = percent / 100 * circumference;
        return `${filled} ${circumference}`;
      },
      /**
       * RSSI 게이지 텍스트 색상
       */
      getGaugeTextColor(rssi) {
        if (rssi <= -90)
          return "text-emerald-600";
        if (rssi <= -80)
          return "text-yellow-600";
        return "text-rose-600";
      },
      /**
       * 스캔 결과에서 주파수 선택
       */
      selectChannelFrequency(freq) {
        this.form.broadcast.frequency = freq;
        this.showToast(`Channel selected: ${Math.round(freq)} MHz - Click "Save Channel Settings" to apply`, "alert-info");
      },
      /**
       * 추천 채널로 이동 (스크롤)
       */
      scrollToChannelSettings() {
        const el = document.getElementById("channel-settings");
        if (el) {
          el.scrollIntoView({ behavior: "smooth" });
        }
      }
    };
  }

  // src/js/modules/devices.js
  function devicesModule() {
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
        sending: null
        // 전송 중인 디바이스 ID
      },
      // 카메라 ID 제어 상태
      cameraIdControl: {
        sending: null
        // 전송 중인 디바이스 ID
      },
      // PING 제어 상태
      pingControl: {
        sending: null
        // 전송 중인 디바이스 ID
      },
      // 상태 요청 제어 상태
      statusControl: {
        sending: false
        // 전송 중 여부
      },
      // 기능 정지 제어 상태
      stopControl: {
        sending: null
        // 전송 중인 디바이스 ID
      },
      // 재부팅 제어 상태
      rebootControl: {
        sending: null
        // 전송 중인 디바이스 ID
      },
      /**
       * 초기화
       */
      async initDevices() {
        await this.fetchDevices();
        this.$watch("currentView", (value) => {
          if (value === "devices") {
            this.startDevicesPolling();
          } else {
            this.stopDevicesPolling();
          }
        });
        if (this.currentView === "devices") {
          this.startDevicesPolling();
        }
      },
      /**
       * 디바이스 목록 조회
       */
      async fetchDevices() {
        try {
          this.devices.loading = true;
          const res = await fetch("/api/devices");
          if (!res.ok)
            throw new Error("Failed to fetch devices");
          const data = await res.json();
          this.devices.list = data.devices || [];
          this.devices.onlineCount = data.count || 0;
          this.devices.registeredCount = data.registeredCount || 0;
        } catch (e) {
          console.error("Devices fetch error:", e);
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
        if (this.devices._pollingTimer)
          return;
        this.devices._pollingTimer = setInterval(async () => {
          await this.fetchDevices();
        }, 5e3);
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
        const brightnessValue = parseInt(brightness, 10);
        if (isNaN(brightnessValue) || brightnessValue < 0 || brightnessValue > 100) {
          console.error("Invalid brightness value:", brightness);
          return;
        }
        const brightness255 = Math.round(brightnessValue * 255 / 100);
        try {
          this.brightnessControl.sending = deviceId;
          const deviceIdBytes = [
            parseInt(deviceId.substring(0, 2), 16),
            parseInt(deviceId.substring(2, 4), 16)
          ];
          const res = await fetch("/api/device/brightness", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({
              deviceId: deviceIdBytes,
              brightness: brightness255
            })
          });
          if (!res.ok) {
            const errorData = await res.json().catch(() => ({}));
            throw new Error(errorData.message || "Failed to set brightness");
          }
          const data = await res.json();
          if (data.status === "ok") {
            console.log(`Brightness set for ${deviceId}: ${brightnessValue}% (${brightness255})`);
            delete this.brightnessControl[deviceId];
            setTimeout(() => this.fetchDevices(), 500);
          } else {
            throw new Error(data.message || "Failed to set brightness");
          }
        } catch (e) {
          console.error("Set brightness error:", e);
          alert(`\uBC1D\uAE30 \uC124\uC815 \uC2E4\uD328: ${e.message}`);
        } finally {
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
          console.error("Invalid camera ID:", cameraId);
          return;
        }
        try {
          this.cameraIdControl.sending = deviceId;
          const deviceIdBytes = [
            parseInt(deviceId.substring(0, 2), 16),
            parseInt(deviceId.substring(2, 4), 16)
          ];
          const res = await fetch("/api/device/camera-id", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({
              deviceId: deviceIdBytes,
              cameraId: cameraIdValue
            })
          });
          if (!res.ok) {
            const errorData = await res.json().catch(() => ({}));
            throw new Error(errorData.message || "Failed to set camera ID");
          }
          const data = await res.json();
          if (data.status === "ok") {
            console.log(`Camera ID set for ${deviceId}: ${cameraIdValue}`);
            delete this.cameraIdControl[deviceId];
            setTimeout(() => this.fetchDevices(), 500);
          } else {
            throw new Error(data.message || "Failed to set camera ID");
          }
        } catch (e) {
          console.error("Set camera ID error:", e);
          alert(`\uCE74\uBA54\uB77C ID \uC124\uC815 \uC2E4\uD328: ${e.message}`);
        } finally {
          this.cameraIdControl.sending = null;
        }
      },
      /**
       * 카메라 ID 조정 (+/- 버튼)
       * @param {string} deviceId - 디바이스 ID
       * @param {number} delta - 변경량 (-1 또는 +1)
       */
      adjustCameraId(deviceId, delta) {
        const currentId = this.cameraIdControl[deviceId] ?? this.devices.list.find((d) => d.id === deviceId)?.cameraId ?? 1;
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
          this.pingControl.sending = deviceId;
          const deviceIdBytes = [
            parseInt(deviceId.substring(0, 2), 16),
            parseInt(deviceId.substring(2, 4), 16)
          ];
          const res = await fetch("/api/device/ping", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({
              deviceId: deviceIdBytes
            })
          });
          if (!res.ok) {
            const errorData = await res.json().catch(() => ({}));
            throw new Error(errorData.message || "Failed to send ping");
          }
          const data = await res.json();
          if (data.status === "ok") {
            console.log(`PING sent to ${deviceId}`);
          } else {
            throw new Error(data.message || "Failed to send ping");
          }
        } catch (e) {
          console.error("PING error:", e);
          alert(`PING \uC2E4\uD328: ${e.message}`);
        } finally {
          this.pingControl.sending = null;
        }
      },
      /**
       * 상태 요청 전송 (Broadcast)
       * 모든 RX 디바이스에 상태 요청 전송
       */
      async requestStatus() {
        try {
          this.statusControl.sending = true;
          const res = await fetch("/api/device/status-request", {
            method: "POST",
            headers: { "Content-Type": "application/json" }
          });
          if (!res.ok) {
            throw new Error("Failed to send status request");
          }
          const data = await res.json();
          if (data.status === "ok") {
            console.log("Status request sent");
          } else {
            throw new Error(data.message || "Failed to send status request");
          }
        } catch (e) {
          console.error("Status request error:", e);
          alert(`\uC0C1\uD0DC \uC694\uCCAD \uC2E4\uD328: ${e.message}`);
        } finally {
          this.statusControl.sending = false;
        }
      },
      /**
       * 디바이스 기능 정지/재개
       * @param {string} deviceId - 디바이스 ID (예: "A1B2")
       */
      async stopDevice(deviceId) {
        try {
          this.stopControl.sending = deviceId;
          const deviceIdBytes = [
            parseInt(deviceId.substring(0, 2), 16),
            parseInt(deviceId.substring(2, 4), 16)
          ];
          const res = await fetch("/api/device/stop", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({
              deviceId: deviceIdBytes
            })
          });
          if (!res.ok) {
            throw new Error("Failed to send stop command");
          }
          const data = await res.json();
          if (data.status === "ok") {
            console.log(`Stop command sent to ${deviceId}`);
            setTimeout(() => this.fetchDevices(), 500);
          } else {
            throw new Error(data.message || "Failed to send stop command");
          }
        } catch (e) {
          console.error("Stop command error:", e);
          alert(`\uAE30\uB2A5 \uC815\uC9C0 \uC2E4\uD328: ${e.message}`);
        } finally {
          this.stopControl.sending = null;
        }
      },
      /**
       * 디바이스 재부팅
       * @param {string} deviceId - 디바이스 ID (예: "A1B2")
       */
      async rebootDevice(deviceId) {
        try {
          if (!confirm(`${deviceId} \uB514\uBC14\uC774\uC2A4\uB97C \uC7AC\uBD80\uD305\uD558\uC2DC\uACA0\uC2B5\uB2C8\uAE4C?`)) {
            return;
          }
          this.rebootControl.sending = deviceId;
          const deviceIdBytes = [
            parseInt(deviceId.substring(0, 2), 16),
            parseInt(deviceId.substring(2, 4), 16)
          ];
          const res = await fetch("/api/device/reboot", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({
              deviceId: deviceIdBytes
            })
          });
          if (!res.ok) {
            throw new Error("Failed to send reboot command");
          }
          const data = await res.json();
          if (data.status === "ok") {
            console.log(`Reboot command sent to ${deviceId}`);
            alert(`${deviceId} \uB514\uBC14\uC774\uC2A4\uC5D0 \uC7AC\uBD80\uD305 \uBA85\uB839\uC744 \uC804\uC1A1\uD588\uC2B5\uB2C8\uB2E4.`);
          } else {
            throw new Error(data.message || "Failed to send reboot command");
          }
        } catch (e) {
          console.error("Reboot command error:", e);
          if (e.message !== "User canceled") {
            alert(`\uC7AC\uBD80\uD305 \uC2E4\uD328: ${e.message}`);
          }
        } finally {
          this.rebootControl.sending = null;
        }
      },
      /**
       * 업타임 포맷 (초 → 읽기 쉬운 형식)
       */
      formatUptime(seconds) {
        if (!seconds)
          return "0s";
        const hours = Math.floor(seconds / 3600);
        const minutes = Math.floor(seconds % 3600 / 60);
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

  // src/js/modules/device.js
  function deviceModule() {
    return {
      /**
       * 디스플레이 밝기 저장
       */
      async saveBrightness() {
        try {
          const res = await fetch("/api/config/device/brightness", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ value: this.form.display.brightness })
          });
          const data = await res.json();
          if (data.status === "ok") {
            this.showToast(`Brightness saved: ${this.form.display.brightness}`, "alert-success");
          } else {
            this.showToast(data.message || "Failed to save brightness", "alert-error");
          }
        } catch (e) {
          this.showToast("Network error. Please try again.", "alert-error");
        }
      },
      /**
       * 카메라 ID 저장
       */
      async saveCameraId() {
        try {
          const res = await fetch("/api/config/device/camera_id", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ value: this.form.display.cameraId })
          });
          const data = await res.json();
          if (data.status === "ok") {
            this.showToast(`Camera ID saved: ${this.form.display.cameraId}`, "alert-success");
          } else {
            this.showToast(data.message || "Failed to save camera ID", "alert-error");
          }
        } catch (e) {
          this.showToast("Network error. Please try again.", "alert-error");
        }
      }
    };
  }

  // src/js/modules/license.js
  function licenseModule() {
    return {
      // 라이센스 데이터
      license: {
        state: 1,
        stateStr: "invalid",
        isValid: false,
        deviceLimit: 0,
        graceRemaining: 0,
        key: "",
        loading: false
      },
      // 라이센스 서버 검색
      licenseSearch: {
        name: "",
        phone: "",
        email: "",
        searching: false,
        result: null,
        success: false
      },
      // 인터넷 테스트 데이터
      internetTest: {
        status: "none",
        // none, testing, success, fail
        serverStatus: "none",
        // none, testing, success, fail
        ping: 0,
        testing: false
      },
      // 라이센스 키 입력 폼
      licenseForm: {
        key: ""
      },
      // 라이센스 상태 매핑
      stateNames: {
        0: "none",
        1: "invalid",
        2: "trial",
        3: "valid",
        4: "grace",
        5: "expired"
      },
      /**
       * 초기화
       */
      async initLicense() {
        await this.fetchLicense();
      },
      /**
       * 라이센스 상태 조회 (/api/status에서 가져옴)
       */
      async fetchLicense() {
        try {
          const res = await fetch("/api/status");
          if (!res.ok)
            throw new Error("Status fetch failed");
          const data = await res.json();
          const lic = data.license || {};
          this.license.state = lic.state ?? 1;
          this.license.stateStr = lic.stateStr || "invalid";
          this.license.isValid = lic.isValid || false;
          this.license.deviceLimit = lic.deviceLimit || 0;
          this.license.graceRemaining = lic.graceRemaining || 0;
          this.license.key = lic.key || "";
          if (lic.key && lic.key.length === 16) {
            this.licenseForm.key = this.formatLicenseKeyString(lic.key);
          }
        } catch (e) {
          console.error("License fetch error:", e);
        }
      },
      /**
       * 라이센스 키 포맷팅 (xxxx-xxxx-xxxx-xxxx)
       */
      formatLicenseKeyString(key) {
        let value = key.replace(/-/g, "").toUpperCase();
        if (value.length > 16) {
          value = value.slice(0, 16);
        }
        if (value.length > 12) {
          value = value.slice(0, 4) + "-" + value.slice(4, 8) + "-" + value.slice(8, 12) + "-" + value.slice(12);
        } else if (value.length > 8) {
          value = value.slice(0, 4) + "-" + value.slice(4, 8) + "-" + value.slice(8);
        } else if (value.length > 4) {
          value = value.slice(0, 4) + "-" + value.slice(4);
        }
        return value;
      },
      /**
       * 인터넷 연결 테스트
       */
      async testInternet() {
        this.internetTest.testing = true;
        this.internetTest.status = "testing";
        this.internetTest.serverStatus = "testing";
        this.internetTest.ping = 0;
        try {
          const dnsRes = await fetch("/api/test/internet", {
            method: "POST",
            headers: { "Content-Type": "application/json" }
          });
          if (dnsRes.ok) {
            const dnsData = await dnsRes.json();
            this.internetTest.status = dnsData.success ? "success" : "fail";
            this.internetTest.ping = dnsData.ping || 0;
          } else {
            this.internetTest.status = "fail";
          }
          const serverRes = await fetch("/api/test/license-server", {
            method: "POST",
            headers: { "Content-Type": "application/json" }
          });
          if (serverRes.ok) {
            const serverData = await serverRes.json();
            this.internetTest.serverStatus = serverData.success ? "success" : "fail";
          } else {
            this.internetTest.serverStatus = "fail";
          }
        } catch (e) {
          console.error("Internet test error:", e);
          this.internetTest.status = "fail";
          this.internetTest.serverStatus = "fail";
        } finally {
          this.internetTest.testing = false;
        }
      },
      /**
       * 라이센스 키 입력 포맷팅 (xxxx-xxxx-xxxx-xxxx)
       */
      formatLicenseKey(event) {
        let value = event.target.value.toUpperCase();
        value = value.replace(/-/g, "");
        value = value.replace(/[^A-Z0-9]/g, "");
        if (value.length > 16) {
          value = value.slice(0, 16);
        }
        if (value.length > 12) {
          value = value.slice(0, 4) + "-" + value.slice(4, 8) + "-" + value.slice(8, 12) + "-" + value.slice(12);
        } else if (value.length > 8) {
          value = value.slice(0, 4) + "-" + value.slice(4, 8) + "-" + value.slice(8);
        } else if (value.length > 4) {
          value = value.slice(0, 4) + "-" + value.slice(4);
        }
        this.licenseForm.key = value;
      },
      /**
       * 라이센스 키 검증 요청
       */
      async validateLicense() {
        const key = this.licenseForm.key.trim();
        if (!key) {
          this.showToast("Please enter license key", "alert-warning");
          return;
        }
        const cleanKey = key.replace(/-/g, "");
        if (cleanKey.length !== 16) {
          this.showToast("License key must be 16 characters (xxxx-xxxx-xxxx-xxxx)", "alert-error");
          return;
        }
        try {
          this.license.loading = true;
          const res = await fetch("/api/validate-license", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ key: cleanKey })
          });
          if (!res.ok) {
            throw new Error("Validation request failed");
          }
          const data = await res.json();
          if (data.status === "accepted") {
            this.showToast("License validation started. Please wait...", "alert-info");
            this.licenseForm.key = "";
            setTimeout(() => this.fetchLicense(), 3e3);
          } else {
            this.showToast("Validation failed: " + (data.error || "Unknown error"), "alert-error");
          }
        } catch (e) {
          console.error("License validate error:", e);
          this.showToast("Validation request failed", "alert-error");
        } finally {
          this.license.loading = false;
        }
      },
      /**
       * 라이센스 서버 검색 (ESP32 경유 - 이름/전화번호/이메일)
       */
      async searchLicenseServer() {
        const { name, phone, email } = this.licenseSearch;
        if (!name || !phone || !email) {
          this.showToast("Please fill in all fields (Name, Phone, Email)", "alert-warning");
          return;
        }
        try {
          this.licenseSearch.searching = true;
          this.licenseSearch.result = null;
          const res = await fetch("/api/search-license", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ name, phone, email })
          });
          if (!res.ok) {
            throw new Error("Search request failed");
          }
          const data = await res.json();
          if (data.success && data.count > 0 && data.licenses && data.licenses.length > 0) {
            this.licenseSearch.success = true;
            const license = data.licenses[0];
            this.licenseSearch.result = `License Key: ${license.license_key}, Device Limit: ${license.device_limit}`;
          } else if (data.success && data.count === 0) {
            this.licenseSearch.success = false;
            this.licenseSearch.result = "No license found for the provided information";
          } else {
            this.licenseSearch.success = false;
            this.licenseSearch.result = data.error || "License not found";
          }
        } catch (e) {
          console.error("License search error:", e);
          this.licenseSearch.success = false;
          this.licenseSearch.result = "Search failed. Please try again.";
        } finally {
          this.licenseSearch.searching = false;
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
          "invalid": "Inactive",
          "trial": "Trial",
          "grace": "Grace Period",
          "checking": "Checking",
          "none": "Not Registered"
        };
        return textMap[this.license.stateStr] || this.license.stateStr.toUpperCase();
      },
      /**
       * 라이센스 상태 색상 클래스 반환
       */
      getLicenseStatusClass() {
        if (this.license.isValid)
          return "text-emerald-600 bg-emerald-50";
        if (this.license.stateStr === "trial")
          return "text-blue-600 bg-blue-50";
        if (this.license.stateStr === "grace")
          return "text-amber-600 bg-amber-50";
        if (this.license.stateStr === "checking")
          return "text-blue-600 bg-blue-50";
        return "text-rose-600 bg-rose-50";
      },
      /**
       * 라이센스 인디케이터 색상 반환
       */
      getLicenseIndicatorClass() {
        if (this.license.isValid)
          return "bg-emerald-500";
        if (this.license.stateStr === "trial")
          return "bg-blue-500";
        if (this.license.stateStr === "grace")
          return "bg-amber-500";
        if (this.license.stateStr === "checking")
          return "bg-blue-500";
        return "bg-rose-500";
      }
    };
  }

  // src/js/modules/utils.js
  function utilsModule() {
    return {
      // 다이얼로그 표시 상태
      showPrimaryConfig: false,
      showSecondaryConfig: false,
      // 토스트 알림
      toast: {
        show: false,
        message: "",
        type: "alert-info"
      },
      /**
       * 토스트 표시
       */
      showToast(msg, type = "alert-info") {
        this.toast.message = msg;
        this.toast.type = type;
        this.toast.show = true;
        setTimeout(() => {
          this.toast.show = false;
        }, 3e3);
      },
      /**
       * 업타임 포맷 (HH:MM:SS)
       */
      formatUptimeShort(seconds) {
        const hours = Math.floor(seconds / 3600);
        const minutes = Math.floor(seconds % 3600 / 60);
        const secs = seconds % 60;
        const hh = String(hours).padStart(2, "0");
        const mm = String(minutes).padStart(2, "0");
        const ss = String(secs).padStart(2, "0");
        return `${hh}:${mm}:${ss}`;
      },
      /**
       * 업타임 포맷
       */
      formatUptime(seconds) {
        const hours = Math.floor(seconds / 3600);
        const minutes = Math.floor(seconds % 3600 / 60);
        if (hours > 0) {
          return `${hours}h ${minutes}m`;
        }
        return `${minutes}m`;
      },
      /**
       * RSSI와 SNR을 고려한 신호 레벨 (0-3)
       * icons.c의 getSignalLevel()과 동일한 로직
       */
      getSignalLevel(rssi, snr) {
        if (rssi > -70 && snr > 5)
          return 3;
        if (rssi > -85 && snr > 0)
          return 2;
        if (rssi > -100 && snr > -5)
          return 1;
        return 0;
      },
      /**
       * 신호 레벨 텍스트
       */
      getSignalText(rssi, snr) {
        const level = this.getSignalLevel(rssi, snr);
        if (level === 3)
          return "Strong";
        if (level === 2)
          return "Good";
        if (level === 1)
          return "Weak";
        return "No Signal";
      },
      /**
       * 바이트 포맷
       */
      formatBytes(bytes) {
        if (bytes < 1024)
          return bytes + " B";
        if (bytes < 1024 * 1024)
          return (bytes / 1024).toFixed(1) + " KB";
        return (bytes / (1024 * 1024)).toFixed(1) + " MB";
      },
      /**
       * 페이지 전환
       */
      goTo(view) {
        this.currentView = view;
      },
      /**
       * 재부팅
       */
      async reboot() {
        if (!confirm("Are you sure you want to reboot the device?"))
          return;
        try {
          await fetch("/api/reboot", { method: "POST" });
          this.showToast("Rebooting device...", "alert-info");
        } catch (e) {
          this.showToast("Failed to reboot", "alert-error");
        }
      }
    };
  }

  // src/js/app.js
  function tallyApp() {
    return {
      // 사이드바 상태
      sidebarOpen: false,
      // 각 모듈 병합
      ...stateModule(),
      ...networkModule(),
      ...switcherModule(),
      ...broadcastModule(),
      ...devicesModule(),
      ...deviceModule(),
      ...licenseModule(),
      ...utilsModule()
    };
  }
  document.addEventListener("alpine:init", () => {
    Alpine.data("tallyApp", tallyApp);
  });
})();
