// Kết nối WebSocket tới server
const ws = new WebSocket(`ws://${location.host}/ws`);

// Gửi lệnh toggle motor/fan
function toggleDevice(device) {
    ws.send(JSON.stringify({ action: "toggle_device", device: device }));
}

// Lắng nghe phản hồi trạng thái thiết bị (nếu cần)
ws.onmessage = function(event) {
    try {
        const msg = JSON.parse(event.data);
        if (msg.type === "device_status") {
            alert(`${msg.device === 'motor' ? 'MOTOR' : 'FAN'} trạng thái: ${msg.state ? 'BẬT' : 'TẮT'}`);
        }
    } catch (e) {}
}

// Gắn vào nút motor và FAN
window.addEventListener('DOMContentLoaded', function() {
    const motorBtn = document.getElementById('motor-btn');
    const fanBtn = document.getElementById('fan-btn');
    if (motorBtn) motorBtn.onclick = () => toggleDevice('motor');
    if (fanBtn) fanBtn.onclick = () => toggleDevice('fan');
});
