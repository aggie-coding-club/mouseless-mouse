#ifndef THREEML_ERROR_H
#define THREEML_ERROR_H

#include <Arduino.h>
#include <cstdint>

namespace threeml {

/// @brief If a condition is true, print an error message to Serial and enter an infinite loop.
/// @param condition The condition to check
/// @param error_msg The error message to print to Serial
/// @param filename Optionally, the name of the file where the error was encountered
/// @param lineno Optionally, the line number where the error was encountered
void maybe_error(bool condition, const char *error_msg, const char *filename = nullptr,
                              std::size_t lineno = 0) noexcept;
/// @brief If a condition is true, print a warning message to Serial.
/// @param condition The condition to check
/// @param warning_msg The warning message to print to Serial
/// @param filename Optionally, the name of the file where the warning was triggered
/// @param lineno Optionally, the line number where the warning was triggered
/// @return Whether or not the warning was triggered
bool maybe_warn(bool condition, const char *warning_msg, const char *filename = nullptr,
                std::size_t lineno = 0) noexcept;

} // namespace threeml

#endif