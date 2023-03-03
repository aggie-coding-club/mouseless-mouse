#ifndef MLM_ERRORS_HPP
#define MLM_ERRORS_HPP

#include <string>
#include <cstdint>

namespace mlm::errors
{
    using flashpattern_t = uint16_t; // 16 bits, each representing 100 milliseconds; 0 is off, 1 is on

    const flashpattern_t HARDWARE_INITIALIZATION_FAILED = 0b1111001100111100;
    const flashpattern_t SOFTWARE_INITIALIZATION_FAILED = 0b1010100010101000;

    /**
     * @brief Suspend all currently-running tasks, log a fatal error message, and show an LED flash pattern
     *        until reset.
     * @param message the message to be logged
     * @param flashPattern an mlm::errors::flashpattern_t corresponding to the desired flash pattern
     */
    [[noreturn]] void doFatalError(const char *message, flashpattern_t flashPattern);
}

#endif