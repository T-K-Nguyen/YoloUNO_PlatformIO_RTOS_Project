let ws = null;
let reconnectTimer = null;

let motorBtn = null;
let fanBtn = null;
let motorStatus = null;
let fanStatus = null;
let motorIcon = null;
let fanIcon = null;

function applyDeviceUI(device, state, fault) {
    const isMotor = device === 'motor';
    const btn = isMotor ? motorBtn : fanBtn;
    const statusEl = isMotor ? motorStatus : fanStatus;
    const iconEl = isMotor ? motorIcon : fanIcon;

    if (!btn || !statusEl || !iconEl) return;

    btn.checked = !!state;

    if (fault) {
        statusEl.textContent = 'Lỗi kết nối / lệch trạng thái';
        statusEl.className = 'mt-3 fw-semibold text-danger';
        iconEl.classList.remove('fa-spin');
        return;
    }

    statusEl.textContent = state ? 'Đang bật' : 'Đang tắt';
    statusEl.className = isMotor
        ? (state ? 'mt-3 fw-semibold text-success' : 'mt-3 fw-semibold text-muted')
        : (state ? 'mt-3 fw-semibold text-info' : 'mt-3 fw-semibold text-muted');

    if (state) {
        iconEl.classList.add('fa-spin');
    } else {
        iconEl.classList.remove('fa-spin');
    }
}

function sendDeviceState(device, state) {
    if (!ws || ws.readyState !== WebSocket.OPEN) {
        return;
    }

    ws.send(JSON.stringify({
        action: 'toggle_device',
        device,
        state: !!state
    }));
}

function scheduleReconnect() {
    if (reconnectTimer) return;
    reconnectTimer = setTimeout(() => {
        reconnectTimer = null;
        connectWebSocket();
    }, 1500);
}

function connectWebSocket() {
    ws = new WebSocket(`ws://${location.host}/ws`);

    ws.onopen = function() {
        console.log('WebSocket connected');
    };

    ws.onmessage = function(event) {
        try {
            const msg = JSON.parse(event.data);

            if (msg.type === 'device_sync') {
                applyDeviceUI(msg.device, !!msg.actual, !!msg.fault);
            }

            if (msg.type === 'device_status') {
                applyDeviceUI(msg.device, !!msg.state, false);
            }

            if (msg.type === 'device_fault') {
                applyDeviceUI(msg.device, !!msg.actual, true);
            }
        } catch (e) {
            console.warn('Invalid WS message', e);
        }
    };

    ws.onclose = function() {
        console.warn('WebSocket disconnected, reconnecting...');
        scheduleReconnect();
    };

    ws.onerror = function() {
        ws.close();
    };
}

window.addEventListener('DOMContentLoaded', function() {
    motorBtn = document.getElementById('motor-btn');
    fanBtn = document.getElementById('fan-btn');
    motorStatus = document.getElementById('motorStatus');
    fanStatus = document.getElementById('fanStatus');
    motorIcon = document.getElementById('motorIcon');
    fanIcon = document.getElementById('fanIcon');

    if (motorBtn) {
        motorBtn.addEventListener('change', (e) => {
            sendDeviceState('motor', e.target.checked);
        });
    }

    if (fanBtn) {
        fanBtn.addEventListener('change', (e) => {
            sendDeviceState('fan', e.target.checked);
        });
    }

    connectWebSocket();
});