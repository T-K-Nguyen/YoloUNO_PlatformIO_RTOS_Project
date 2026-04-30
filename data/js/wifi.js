
const ws = new WebSocket(`ws://${location.host}/ws`); // Kết nối WebSocket

document.getElementById('wifi-config-form').addEventListener('submit', async function(e) {
    e.preventDefault();
    const ssid = document.getElementById('ssid').value;
    const password = document.getElementById('password').value;
    // DISABLED (Part 2): Local MQTT Broker config - only testing CoreIOT in Part 1
    // const localMqttIp = document.getElementById('local-mqtt-ip').value;
    // const localMqttPort = document.getElementById('local-mqtt-port').value || '1883';

    if (!ssid.trim() || !password.trim()) {
        alert("Vui lòng điền SSID và password!");
        return;
    }
    alert(`Đã gửi tín hiệu kết nối với WIFI ${ssid}`);  
    ws.send(JSON.stringify({
        action: "wifi",
        ssid: ssid,
        password: password
        // DISABLED (Part 2): Local MQTT fields
        // local_mqtt_ip: localMqttIp,
        // local_mqtt_port: localMqttPort
    }));
});

// Lắng nghe phản hồi từ server để alert khi kết nối WiFi thành công/thất bại
ws.onmessage = function(event) {
    try {
        const msg = JSON.parse(event.data);
        if (msg.type === "wifi_connected") {
            if (msg.success) {
                alert("Kết nối WiFi thành công! Đang chuyển về trang chủ...");
                window.location.href = "/";
            } else {
                alert("Kết nối WiFi thất bại! Vui lòng kiểm tra lại thông tin.");
            }
        }
    } catch (e) {}
}

function checkInputs(formId, buttonId) {
    const form = document.getElementById(formId);
    const connectBtn = document.getElementById(buttonId);
    const inputs = form.querySelectorAll('input[required]');

    let allFilled = true;
    inputs.forEach(input => {
        if (!input.value.trim()) allFilled = false;
    });
    connectBtn.disabled = !allFilled;
}

document.querySelectorAll('input[required]').forEach(input => {
    input.addEventListener('input', function() {
        checkInputs(input.closest('form').id, input.closest('form').querySelector('button').id);
    });
});

window.addEventListener('DOMContentLoaded', function() {
    checkInputs('wifi-config-form', 'wifi-connect-btn');
});