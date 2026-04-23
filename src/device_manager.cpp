#include "device_manager.h"

static const int FAN1_PIN = 38;
static const int FAN2_PIN = 8;

static Device devices[] = {
    {"fan1", FAN1_PIN, false, false, DEVICE_OFF, false},
    {"fan2", FAN2_PIN, false, false, DEVICE_OFF, false},
};

static const size_t DEVICE_COUNT = sizeof(devices) / sizeof(devices[0]);

static void initDevice(Device &device)
{
    if (device.initialized)
        return;

    pinMode(device.pin, OUTPUT);
    digitalWrite(device.pin, LOW);
    device.desired = false;
    device.actual = false;
    device.status = DEVICE_OFF;
    device.initialized = true;
}
// Hàm khởi tạo tất cả thiết bị
void deviceManagerInitAll()
{
    for (size_t i = 0; i < DEVICE_COUNT; ++i)
    {
        initDevice(devices[i]);
    }
}
// Hàm lấy con trỏ đến thiết bị theo tên, trả về nullptr nếu không tìm thấy
Device *deviceManagerGet(const String &name)
{
    for (size_t i = 0; i < DEVICE_COUNT; ++i)
    {
        if (name == devices[i].name)
        {
            initDevice(devices[i]);
            return &devices[i];
        }
    }
    return nullptr;
}
// Hàm lấy con trỏ đến thiết bị theo chỉ số, trả về nullptr nếu chỉ số không hợp lệ
Device *deviceManagerGetAt(size_t index)
{
    if (index >= DEVICE_COUNT)
        return nullptr;

    initDevice(devices[index]);
    return &devices[index];
}

size_t deviceManagerCount()
{
    return DEVICE_COUNT;
}
// Hàm áp dụng trạng thái mong muốn cho thiết bị, cập nhật trạng thái thực tế và tổng hợp
void deviceManagerApplyDesiredState(Device &device, bool desiredState)
{
    device.desired = desiredState;
    digitalWrite(device.pin, device.desired ? HIGH : LOW);

    // Trust command path to avoid false mismatch detection.
    device.actual = device.desired;
    device.status = device.actual ? DEVICE_ON : DEVICE_OFF;
}
