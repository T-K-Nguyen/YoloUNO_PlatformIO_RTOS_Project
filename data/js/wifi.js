const ws = new WebSocket(`ws://${location.host}/ws`);

function getInputValue(...ids) {
	for (const id of ids) {
		const el = document.getElementById(id);
		if (el && typeof el.value === "string") {
			return el.value.trim();
		}
	}
	return "";
}

function getMqttConfig() {
	const localMqttIp = getInputValue("localMqttIp", "local-mqtt-ip");
	const localMqttPort = getInputValue("localMqttPort", "local-mqtt-port") || "1883";
	return { localMqttIp, localMqttPort };
}

const wifiForm = document.getElementById("wifi-config-form");
if (wifiForm) {
	wifiForm.addEventListener("submit", function (e) {
		e.preventDefault();

		const ssid = getInputValue("ssid");
		const password = document.getElementById("password")?.value || "";
		const { localMqttIp, localMqttPort } = getMqttConfig();

		if (!ssid || !password.trim() || !localMqttIp) {
			alert("Vui long dien SSID, password va Local MQTT Broker IP!");
			return;
		}

		// Save all in one user action: WiFi + MQTT together.
		ws.send(JSON.stringify({
			action: "wifi",
			ssid: ssid,
			password: password,
			local_mqtt_ip: localMqttIp,
			local_mqtt_port: localMqttPort
		}));

		// Also trigger dedicated mqtt_config action so backend path is covered.
		ws.send(JSON.stringify({
			action: "mqtt_config",
			local_mqtt_ip: localMqttIp,
			local_mqtt_port: localMqttPort
		}));
	});
}

ws.onmessage = function (event) {
	try {
		const msg = JSON.parse(event.data);

		if (msg.type === "wifi_config_saved") {
			if (msg.success) {
				alert("Da luu cau hinh WiFi + MQTT.");
			} else {
				alert("Luu cau hinh WiFi that bai.");
			}
			if (msg.warning) {
				alert(msg.warning);
			}
			return;
		}

		if (msg.type === "wifi_connected" && msg.success) {
			alert("Ket noi WiFi thanh cong!");
			return;
		}

		if (msg.type === "wifi_disconnected") {
			alert("WiFi bi ngat ket noi.");
			return;
		}

		if (msg.type === "mqtt_config_saved") {
			if (msg.success) {
				alert("Luu Local MQTT config thanh cong!");
			} else {
				alert("Luu Local MQTT config that bai.");
			}
			if (msg.warning) {
				alert(msg.warning);
			}
			return;
		}
	} catch (e) {
	}
};

function checkInputs(formId, buttonId) {
	const form = document.getElementById(formId);
	const button = document.getElementById(buttonId);
	if (!form || !button) {
		return;
	}

	const inputs = form.querySelectorAll("input[required]");
	let allFilled = true;
	inputs.forEach((input) => {
		if (!input.value.trim()) {
			allFilled = false;
		}
	});
	button.disabled = !allFilled;
}

document.querySelectorAll("input[required]").forEach((input) => {
	input.addEventListener("input", function () {
		const form = input.closest("form");
		if (!form) {
			return;
		}
		const button = form.querySelector("button[type='submit']");
		if (!button || !button.id) {
			return;
		}
		checkInputs(form.id, button.id);
	});
});

window.addEventListener("DOMContentLoaded", function () {
	checkInputs("wifi-config-form", "wifi-connect-btn");
});
