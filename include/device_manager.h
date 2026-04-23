#ifndef DEVICE_MANAGER_H
#define DEVICE_MANAGER_H

#include <Arduino.h>

enum DeviceStatus
{
    DEVICE_OFF = 0,
    DEVICE_ON = 1,
};

struct Device
{
    const char *name;
    uint8_t pin;         // Chân GPIO điều khiển thiết bị
    bool desired;        // Trạng thái mong muốn (do người dùng yêu cầu)
    bool actual;         // Trạng thái thực tế (đọc từ GPIO)
    DeviceStatus status; // Trạng thái tổng hợp (ON/OFF/ERROR)
    bool initialized;    // Đánh dấu đã khởi tạo chân GPIO hay chưa
};

void deviceManagerInitAll();
Device *deviceManagerGet(const String &name);
Device *deviceManagerGetAt(size_t index);
size_t deviceManagerCount();
void deviceManagerApplyDesiredState(Device &device, bool desiredState);

#endif
