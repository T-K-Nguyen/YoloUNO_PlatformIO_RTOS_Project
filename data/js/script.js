document.addEventListener('DOMContentLoaded', () => {
    // 1. Mobile Sidebar Toggle
    const sidebar = document.getElementById('sidebar');
    const sidebarToggle = document.getElementById('sidebarToggle');
    
    if (sidebarToggle) {
        sidebarToggle.addEventListener('click', () => {
            sidebar.classList.toggle('show');
        });
    }

    // 2. MQTT Form Handle (Config Page)
    // Phần cấu hình WiFi đã được chuyển sang wifi.js
    const mqttForm = document.getElementById('mqttForm');

    if (mqttForm) {
        mqttForm.addEventListener('submit', (e) => {
            e.preventDefault();
            const server = document.getElementById('mqttServer').value;
            const token = document.getElementById('mqttToken').value;
            const port = document.getElementById('mqttPort').value;
            console.log('Lưu cấu hình MQTT:', { server, token, port });
            
            alert('Đã lưu cấu hình MQTT!');
        });
    }
});
