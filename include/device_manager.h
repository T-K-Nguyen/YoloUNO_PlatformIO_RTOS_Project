#ifndef DEVICE_MANAGER_H
#define DEVICE_MANAGER_H

#include <Arduino.h>

enum DeviceStatus
{
    DEVICE_OFF = 0,
    DEVICE_ON = 1,
    DEVICE_ERROR = 2,
};

struct Device
{
    const char *name;
    uint8_t pin;
    bool desired;
    bool actual;
    DeviceStatus status;
    bool initialized;
};

void deviceManagerInitAll();
Device *deviceManagerGet(const String &name);
Device *deviceManagerGetAt(size_t index);
size_t deviceManagerCount();
void deviceManagerApplyDesiredState(Device &device, bool desiredState);

#endif
