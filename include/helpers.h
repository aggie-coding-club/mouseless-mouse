#ifndef IM_SORRY
#define IM_SORRY

#include <Arduino.h>

struct PositiveModulo {
    PositiveModulo(int32_t modulus) : modulus(modulus) {}
    int32_t modulus;
};

// Let's face it: The way C++ handles modular division is stupid.
PositiveModulo operator ""_pm (unsigned long long modulus);

// Is it too much to ask for math operators to do real math?
uint32_t operator %(int32_t lhs, PositiveModulo rhs);

template <typename T>
T operator %=(T& lhs, PositiveModulo rhs) {
    return lhs = lhs % rhs;
}

#endif