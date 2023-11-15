#include <BleMouse.h>

class CustomBLEMouse : public BleMouse {
    private:
        TaskHandle_t handle;

    public:
    CustomBLEMouse(std::string deviceName, std::string deviceManufacturer);

    void begin();

    void end();
};