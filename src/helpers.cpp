#include <Arduino.h>
#include "helpers.h"

PositiveModulo operator ""_pm (unsigned long long modulus) {
    return PositiveModulo ((int32_t)modulus);
}

uint32_t operator %(int32_t lhs, PositiveModulo rhs) {
    int32_t result = lhs % rhs.modulus;
    if (result < 0) result += rhs.modulus;
    return result;
}