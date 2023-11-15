#include <BleMouse.h>
#include <BLEDevice.h>

class CustomBLEMouse : public BleMouse {
    private:
        TaskHandle_t handle;

    public:
    CustomBLEMouse(std::string deviceName, std::string deviceManufacturer) : BleMouse(deviceName, deviceManufacturer, 100) {};

    void begin() {
        BleMouse::begin();
        handle = xTaskGetHandle("server");
    }

    void end() {
        vTaskDelete(handle);
        BLEDevice::
        BLEDevice::deinit(true);
    }
};