#include "helpers.h"
#include <Arduino.h>

// Wrap an integer using the _pm literal with the PositiveModulo class
PositiveModulo operator""_pm(unsigned long long modulus) {
  return PositiveModulo((int32_t)modulus);
}

// Implement a mathematically accurate modulo operator
uint32_t operator%(int32_t lhs, PositiveModulo rhs) {
  int32_t result = lhs % rhs.modulus;
  if (result < 0)
    result += rhs.modulus;
  return result;
}