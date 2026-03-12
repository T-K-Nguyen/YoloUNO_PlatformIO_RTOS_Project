function updateEnvFromSensors() {

    const tempEl = document.getElementById("temperature");
    const humEl = document.getElementById("humidity");

    if (!tempEl || !humEl) return;

    fetch("/sensors")
        .then(res => res.json())
        .then(data => {

            if (typeof data.temp === "number") {
                tempEl.textContent = data.temp.toFixed(1) + " °C";
            }

            if (typeof data.hum === "number") {
                humEl.textContent = data.hum.toFixed(1) + " %";
            }

        })
        .catch(err => console.error("Sensor error:", err));

}