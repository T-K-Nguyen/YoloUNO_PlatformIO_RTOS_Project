function updateDeviceUI(data) {

    const devices = ['led1','led2','pump','fan'];

    devices.forEach(device => {

        const statusElement = document.getElementById(device + 'Status');
        const iconWrapper = document.getElementById(device + 'Icon');

        if (data[device] && statusElement && iconWrapper) {

            const status = data[device];

            statusElement.innerText = status;

            if (status === "ON") {
                iconWrapper.classList.add("led-on");
                iconWrapper.classList.remove("led-off");
            } else {
                iconWrapper.classList.add("led-off");
                iconWrapper.classList.remove("led-on");
            }

        }

    });
}


function toggleDevice(device, state) {

    let ledId = 0;

    if (device === 'led1') ledId = 1;
    else if (device === 'led2') ledId = 2;
    else if (device === 'pump') ledId = 'pump';
    else if (device === 'fan') ledId = 'fan';
    else return;

    fetch(`/set?led=${ledId}&state=${state}`)
        .then(res => res.json())
        .then(data => updateDeviceUI(data))
        .catch(err => console.error("Device error:", err));

}


function pollDeviceState(){

    fetch('/state')
        .then(res => res.json())
        .then(data => updateDeviceUI(data))
        .catch(err => console.error("State polling error:", err));

}