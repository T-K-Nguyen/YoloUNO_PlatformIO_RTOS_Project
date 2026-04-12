let ws = null;
let reconnectTimer = null;

let fan1Btn = null;
let fan2Btn = null;
let fan1Status = null;
let fan2Status = null;
let fan1Icon = null;
let fan2Icon = null;

function applyDeviceUI(device, state, fault) {
    const isFan1 = device === 'fan1';
    const btn = isFan1 ? fan1Btn : fan2Btn;
    const statusEl = isFan1 ? fan1Status : fan2Status;
    const iconEl = isFan1 ? fan1Icon : fan2Icon;

    if (!btn || !statusEl || !iconEl) return;

    btn.checked = !!state;

    if (fault) {
        statusEl.textContent = 'Lỗi kết nối / lệch trạng thái';
        statusEl.className = 'mt-3 fw-semibold text-danger';
        iconEl.classList.remove('fa-spin');
        return;
    }

    statusEl.textContent = state ? 'Đang bật' : 'Đang tắt';
    statusEl.className = isFan1
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
    fan1Btn = document.getElementById('fan1-btn');
    fan2Btn = document.getElementById('fan2-btn');
    fan1Status = document.getElementById('fan1Status');
    fan2Status = document.getElementById('fan2Status');
    fan1Icon = document.getElementById('fan1Icon');
    fan2Icon = document.getElementById('fan2Icon');

    if (fan1Btn) {
        fan1Btn.addEventListener('change', (e) => {
            sendDeviceState('fan1', e.target.checked);
        });
    }

    if (fan2Btn) {
        fan2Btn.addEventListener('change', (e) => {
            sendDeviceState('fan2', e.target.checked);
        });
    }

    connectWebSocket();
});