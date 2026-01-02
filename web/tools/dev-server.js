#!/usr/bin/env node
/**
 * 개발용 HTTP 서버
 * 사용: npm run dev
 */

import http from 'http';
import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const SRC_DIR = path.resolve(__dirname, '../src');

const MIME_TYPES = {
    '.html': 'text/html',
    '.css': 'text/css',
    '.js': 'text/javascript',
    '.mjs': 'text/javascript',
    '.json': 'application/json',
    '.png': 'image/png',
    '.jpg': 'image/jpeg',
    '.svg': 'image/svg+xml',
    '.ico': 'image/x-icon'
};

// ============================================================================
// 더미 API 데이터
// ============================================================================

const DUMMY_DEVICES = [
    {
        id: 'A1B2',
        rssi: -65,
        snr: 12,
        battery: 85,
        cameraId: 1,
        uptime: 3665,
        brightness: 80,
        stopped: false,
        ping: 120,
        frequency: 868,
        syncWord: 0x12
    },
    {
        id: 'C3D4',
        rssi: -78,
        snr: 8,
        battery: 42,
        cameraId: 2,
        uptime: 7200,
        brightness: 65,
        stopped: false,
        ping: 180,
        frequency: 868,
        syncWord: 0x12
    },
    {
        id: 'E5F6',
        rssi: -92,
        snr: 3,
        battery: 15,
        cameraId: 3,
        uptime: 54000,
        brightness: 90,
        stopped: false,
        ping: 450,
        frequency: 868,
        syncWord: 0x12
    }
];

const DUMMY_API = {
    '/api/status': {
        network: {
            ap: {
                enabled: true,
                ssid: 'Tally-Node',
                channel: 1,
                ip: '192.168.4.1'
            },
            wifi: {
                enabled: true,
                ssid: 'WiFi-Network',
                connected: false,
                ip: '--'
            },
            ethernet: {
                enabled: false,
                dhcp: true,
                staticIp: '',
                netmask: '',
                gateway: '',
                connected: false,
                detected: false,
                ip: '--'
            }
        },
        switcher: {
            primary: {
                connected: true,
                type: 'ATEM',
                ip: '192.168.1.100',
                port: 9910,
                interface: 2,
                cameraLimit: 0,
                tally: {
                    pgm: [1, 3],
                    pvw: [2, 5],
                    raw: '01020000000000000000000000000000',
                    channels: 8
                }
            },
            secondary: {
                connected: false,
                type: 'vMix',
                ip: '',
                port: 8088,
                interface: 1,
                cameraLimit: 0,
                tally: {
                    pgm: [],
                    pvw: [],
                    raw: '',
                    channels: 0
                }
            },
            dualEnabled: false,
            secondaryOffset: 0
        },
        system: {
            deviceId: 'TEST01',
            battery: 75,
            voltage: 3.8,
            temperature: 32.5,
            uptime: 12345,
            loraChipType: 1
        },
        config: {
            device: {
                brightness: 100,
                cameraId: 1
            },
            rf: {
                frequency: 868,
                syncWord: 0x12,
                spreadingFactor: 7,
                codingRate: 1
            }
        }
    },
    '/api/devices': {
        count: 3,
        registeredCount: 3,
        devices: DUMMY_DEVICES
    }
};

const server = http.createServer((req, res) => {
    // API 요청 처리 (GET만 지원)
    if (req.url.startsWith('/api/')) {
        const dummyData = DUMMY_API[req.url];
        if (dummyData) {
            res.writeHead(200, { 'Content-Type': 'application/json' });
            res.end(JSON.stringify(dummyData), 'utf-8');
            console.log(`API: ${req.url} → 200 OK`);
        } else {
            res.writeHead(404, { 'Content-Type': 'application/json' });
            res.end(JSON.stringify({ error: 'Not Found' }), 'utf-8');
            console.log(`API: ${req.url} → 404 Not Found`);
        }
        return;
    }

    // 정적 파일 처리 - src 폴더에서 제공
    let filePath = path.join(SRC_DIR, req.url === '/' ? 'index.html' : req.url);

    // 디렉토리 요청 시 index.html
    try {
        if (fs.statSync(filePath).isDirectory()) {
            filePath = path.join(filePath, 'index.html');
        }
    } catch (e) {
        // 파일/디렉토리가 없으면 404
        res.writeHead(404, { 'Content-Type': 'text/html' });
        res.end('<h1>404 - Not Found</h1>', 'utf-8');
        return;
    }

    const ext = path.extname(filePath);
    const contentType = MIME_TYPES[ext] || 'application/octet-stream';

    fs.readFile(filePath, (err, content) => {
        if (err) {
            if (err.code === 'ENOENT') {
                res.writeHead(404, { 'Content-Type': 'text/html' });
                res.end('<h1>404 - Not Found</h1>', 'utf-8');
            } else {
                res.writeHead(500);
                res.end(`Server Error: ${err.code}`, 'utf-8');
            }
        } else {
            res.writeHead(200, { 'Content-Type': contentType });
            res.end(content, 'utf-8');
        }
    });
});

const PORT = 8081;
server.listen(PORT, () => {
    console.log(`\n🚀 Development server running at http://localhost:${PORT}/`);
    console.log(`📁 Serving files from: ${SRC_DIR}`);
    console.log(`📡 Dummy API enabled\n`);
});
