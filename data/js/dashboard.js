function initDashboard(){

    updateEnvFromSensors();
    pollDeviceState();
    updateWifiInfo();
    updateTinyMLInfo();

    setInterval(updateEnvFromSensors,2000);
    setInterval(pollDeviceState,1000);
    setInterval(updateWifiInfo,3000);
    setInterval(updateTinyMLInfo,3000);

}