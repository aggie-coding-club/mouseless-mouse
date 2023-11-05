#include "3ml_error.h"

namespace threeml {

void maybe_error(bool condition, const char *error_msg, const char *filename, std::size_t lineno) noexcept {
  if (condition) {
    return;
  }

  Serial.print("ERROR ENCOUNTERED");
  if (filename != nullptr) {
    Serial.printf(" IN FILE \"%s\"", filename);
  }
  if (lineno != 0) {
    Serial.printf(" AT LINE %u", lineno);
  }
  Serial.printf(": %s\n", error_msg);

  for (;;) {
  }
}

bool maybe_warn(bool condition, const char *warning_msg, const char *filename, std::size_t lineno) noexcept {
  if (condition) {
    return;
  }

  Serial.print("WARNING EMITTED");
  if (filename != nullptr) {
    Serial.printf(" IN FILE \"%s\"", filename);
  }
  if (lineno != 0) {
    Serial.printf(" AT LINE %u", lineno);
  }
  Serial.printf(": %s\n", warning_msg);

  return !condition;
}

} // namespace threeml