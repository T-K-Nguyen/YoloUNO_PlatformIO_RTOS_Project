function openWifiSettings() {
    window.location.href = '/wifi.html';
}

function applyWifiStatusUI(rawStatus, ssid){

    const statusBtn = document.getElementById("wifiStatusDashboard");
    const ssidEl = document.getElementById("wifiSsidDashboard");

    if (!statusBtn || !ssidEl) return;

    const s = (rawStatus || "").toLowerCase();

    let label = "Unknown";
    let className = "status-unknown";

    if(s==="connected"){
        label="Connected";
        className="status-connected";
    }

    if(s==="connecting"){
        label="Connecting...";
        className="status-connecting";
    }

    if(s==="failed" || s==="disconnected"){
        label="Disconnected";
        className="status-failed";
    }

    statusBtn.className = className;
    statusBtn.textContent = label;

    ssidEl.textContent = ssid ? `SSID: ${ssid}` : "SSID: --";
}


function updateWifiInfo(){

    fetch("/wifi_info")
        .then(res => res.json())
        .then(info => {
            applyWifiStatusUI(info.status, info.ssid);
        })
        .catch(err => console.error("wifi_info error:", err));

}