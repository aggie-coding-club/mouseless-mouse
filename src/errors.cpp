#include "errors.hpp"

#include <ArduinoLog.h>
#include <string>

namespace mlm::errors
{
    [[noreturn]] void doFatalError(std::string message, flashpattern_t flashPattern)
    {
        vTaskSuspendAll();
        Log.fatalln(message);
        pinMode(BUILTIN_LED, OUTPUT);
        for (;;)
        {
            for (size_t i = 0; i < 16; i++)
            {
                digitalWrite(LED_BUILTIN, ((flashPattern >> i) & 0x01) ? HIGH : LOW);
                delay(100);
            }
        }
    }
}