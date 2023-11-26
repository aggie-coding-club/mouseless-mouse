#include "CustomBLEMouse.h"
#include <BleMouse.h>
#include <BLEDevice.h>


CustomBLEMouse::CustomBLEMouse(std::string deviceName, std::string deviceManufacturer) : BleMouse(deviceName, deviceManufacturer, 100) {};

void CustomBLEMouse::begin() {
    BleMouse::begin();
    handle = xTaskGetHandle("server");
}

void CustomBLEMouse::end() {
    vTaskDelete(handle);
    BLEDevice::deinit(true);
}