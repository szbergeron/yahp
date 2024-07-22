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

template<typename T>
inline T min(T a, T b) {
    if (a < b) {
        return a;
    } else {
        return b;
    }
}

template<typename T>
inline T max(T a, T b) {
    if (a > b) {
        return a;
    } else {
        return b;
    }
}

void print_bounds(int value, int upper, int lower) {
  Serial.print(value);
  value = value > upper ? upper : value;
  value = value - lower;
  
  int res = 1;

  Serial.print("\xE2\x96\x88");
  
  for(int i = 0; i < value; i++) {
    if (i % res == 0) {
      Serial.print("\xE2\x96\x88");
    }
  }
  
  for(int i = value; i < upper; i++) {
    if( i % res == 0) {
      //Serial.print(" ");
    }
  }

  Serial.print("\xE2\x96\x88");
  
  Serial.print("\r\n");
}

void print_value(int value) {
  //int min = 200;
  //int max = 500;
  int lower = 210;
  int upper = 550;
  print_bounds(value, upper, lower);

}

void gap(uint32_t a, uint32_t b, String m) {
    Serial.println("Gap: " + m + " | " + String(b - a));
}


#endif
