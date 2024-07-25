#ifndef YAHP_UTILS
#define YAHP_UTILS

#include "core_pins.h"
#include <Arduino.h>
#include <cstdint>
#include <math.h>

[[gnu::noinline]]
[[gnu::cold]]
[[gnu::unused]]
static void eloop(String s) {
    while(true) {
        Serial.println("A failure mode was entered due to an unrecoverable circumstance:");
        Serial.println(s);
        delay(10000);
    }
}



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

static inline void set_board(uint_fast8_t val) {
    uint8_t starting_pin = 5;

    // I am a dumbass and put the bits backwards in the board layout
    for(uint8_t bit = 0; bit < 8; bit++) {
        uint8_t bitval = (val >> (7 - bit)) & 0x1;
        digitalWriteFaster(bitval, starting_pin + bit);
    }
}

[[gnu::noinline]]
[[gnu::cold]]
[[gnu::unused]]
static void errorln(const char *s) { Serial.println(s); }

template<typename T>
[[gnu::unused]]
inline T min(T a, T b) {
    if (a < b) {
        return a;
    } else {
        return b;
    }
}

template<typename T>
[[gnu::unused]]
inline T max(T a, T b) {
    if (a > b) {
        return a;
    } else {
        return b;
    }
}

int32_t myabs(int32_t v) {
    if (v < 0) {
        return -v;
    } else {
        return v;
    }
}

[[gnu::unused]]
static void print_bounds(int value, int upper, int lower, bool term) {
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
  
  if(term) {
    Serial.print("\r\n");
  }
}

[[gnu::unused]]
static void print_value(int value, bool term) {
  //int min = 200;
  //int max = 500;
  int lower = 0;
  int upper = 1000;
  print_bounds(value, upper, lower, term);

}

[[gnu::unused]]
static void gap(uint32_t a, uint32_t b, String m) {
    Serial.println("Gap: " + m + " | " + String(b - a));
}


#endif
