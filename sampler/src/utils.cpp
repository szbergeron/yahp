#ifndef YAHP_UTILS
#define YAHP_UTILS

#include "core_pins.h"
#include <Arduino.h>
#include <cstdint>
#include <math.h>


// I'm a bastard and a curse upon this world, but
// if you're gonna mock me for this
// then feel free to write something faster
//
// also, no, I'm not gonna force any attrs on here
// because compilers are all grown up and can
// make their own decisions
static inline void digitalWriteFaster(uint_fast8_t val, uint_fast8_t pin) {
    uint8_t adder = val > 0;
    *(digital_pin_to_info_PGM[(pin)].reg + 1 - adder) = digital_pin_to_info_PGM[(pin)].mask;
}

void errorln(const char *s) { Serial.println(s); }

#endif
