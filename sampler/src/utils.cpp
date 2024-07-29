#ifndef YAHP_UTILS
#define YAHP_UTILS

#include <Arduino.h>
#include "core_pins.h"
#include <cstdint>
#include <math.h>

#include <ArduinoJson.hpp>
#include "ArduinoJson/Json/PrettyJsonSerializer.hpp"
#include "ArduinoJson/Variant/JsonVariantConst.hpp"
#include "ArduinoJson/Object/JsonObject.hpp"

using namespace ArduinoJson;

[[gnu::noinline]] [[gnu::cold]] [[gnu::unused]] static void eloop(String s) {
  while (true) {
    Serial.println(
        "A failure mode was entered due to an unrecoverable circumstance:");
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
  *(digital_pin_to_info_PGM[(pin)].reg + 1 - adder) =
      digital_pin_to_info_PGM[(pin)].mask;
}

static inline void set_board(uint_fast8_t val) {
  uint8_t starting_pin = 5;

  // I am a dumbass and put the bits backwards in the board layout
  for (uint8_t bit = 0; bit < 8; bit++) {
    uint8_t bitval = (val >> (7 - bit)) & 0x1;
    digitalWriteFaster(bitval, starting_pin + bit);
  }
}

[[gnu::noinline]] [[gnu::cold]] [[gnu::unused]] static void
errorln(const char *s) {
  Serial.println(s);
}

template <typename T> [[gnu::unused]] inline T min(T a, T b) {
  if (a < b) {
    return a;
  } else {
    return b;
  }
}

template <typename T> [[gnu::unused]] inline T max(T a, T b) {
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

float myabsf(float v) {
  if (v < 0) {
    return -v;
  } else {
    return v;
  }
}

[[gnu::unused]] static void print_bounds(int value, int upper, int lower,
                                         bool term) {
  Serial.print(value);
  value = value > upper ? upper : value;
  value = value - lower;

  int res = 1;

  Serial.print("\xE2\x96\x88");

  for (int i = 0; i < value; i++) {
    if (i % res == 0) {
      Serial.print("\xE2\x96\x88");
    }
  }

  for (int i = value; i < upper; i++) {
    if (i % res == 0) {
      // Serial.print(" ");
    }
  }

  Serial.print("\xE2\x96\x88");

  if (term) {
    Serial.print("\r\n");
  }
}

[[gnu::unused]] static void print_value(int value, bool term) {
  // int min = 200;
  // int max = 500;
  int lower = 0;
  int upper = 300;
  //
  print_bounds(value, upper, lower, term);
}

[[gnu::unused]] static void print_normalized(float v) {
  // int min = 200;
  // int max = 500;
  int upper = 120;
  int lower = 0;

  float fvalue = v;
  Serial.print(fvalue);
  int value = fvalue * upper;
  value = value > upper ? upper : value;
  value = value - lower;

  int res = 1;

  Serial.print(("\xE2\x96\x88"));

  for (int i = 0; i < value; i++) {
    if (i % res == 0) {
      Serial.print(("\xE2\x96\x88"));
    }
  }

  for (int i = value; i < upper; i++) {
    if (i % res == 0) {
      // Serial.print(" ");
    }
  }

  Serial.print(("\xE2\x96\x88"));
}

/*[[gnu::unused]]
static void gap(uint32_t a, uint32_t b, String m) {
    Serial.println(F("Gap: ") + m + F(" | ") + String(b - a));
}*/

const uint8_t FIRST_MIDI_NOTE = 21;

String format_note(uint8_t index) {
  String base;

  auto midi_notenum = index - FIRST_MIDI_NOTE;

  auto midi_octave = midi_notenum / 12;
  auto midi_note = midi_notenum % 12;

  switch (midi_note) {
  case 0:
    base.append("A");
    break;
  case 1:
    base.append("A#/Bb");
    break;
  case 2:
    base.append("B");
    break;
  case 3:
    base.append("C");
    break;
  case 4:
    base.append("C#/Db");
    break;
  case 5:
    base.append("D");
    break;
  case 6:
    base.append("D#/Eb");
    break;
  case 7:
    base.append("E");
    break;
  case 8:
    base.append("F");
    break;
  case 9:
    base.append("F#/Gb");
    break;
  case 10:
    base.append("G");
    break;
  case 11:
    base.append("G#/Ab");
    break;
  }

  base.append(" ");

  base.append(midi_octave);

  return base;
}

bool newline_waiting() {
  bool val = Serial.read() == '\r' || Serial.read() == '\n';
  Serial.clear();

  return val;
}

String read_line() {
  char bytes[128];
  // auto num = Serial.readBytesUntil('\n', bytes, 128);
  size_t len = 0;

  while (true) {
    while (!Serial.available()) {
      delay(1);
    }

    if (len >= 127) {
      break;
    }

    int v = Serial.read();

    if (v == -1) {
      delay(1);
      continue;
    } else if (v == '\n' || v == '\r') {
      Serial.println();
      // Serial.println("\r\ngot it!");
      break;
    } else {
      bytes[len++] = v;
      Serial.printf("%c", (char)v);
    }
  }

  bytes[len] = 0;

  auto res = String(bytes);
  // Serial.println("Got line:" + res);

  return res;
}

bool confirm(String message, bool default_val) {
  Serial.print(message);
  if (default_val) {
    Serial.print(" [Y/n]: ");
  } else {
    Serial.print(" [y/N]: ");
  }

  Serial.flush();
  while (true) {
    auto s = read_line();

    // auto s = Serial.readStringUntil('\n');
    if (s == "n" || s == "N") {
      return false;
    } else if (s == "y" || s == "Y") {
      return true;
    } else if (s == "") {
      return default_val;
    } else if (s == -1) {
      delay(3);
      continue;
    } else {
      Serial.println("Unrecognized value: " + s);
    }

    if (default_val) {
      Serial.print(message + " (Y/n): ");
    } else {
      Serial.print(message + " (y/N): ");
    }
  }
}

int32_t prompt_int(String message, int32_t min, int32_t max,
                   int32_t default_val) {
  int32_t val = default_val;
  while (true) {
    Serial.print(message + " [" + default_val + "]: ");

    auto line = read_line();
    // toInt is absolutely stupid and broken, so I guess I'm doing it myself
    int32_t ret = 0;
    if (line.c_str()[0] == 0) {
      Serial.printf("Selected default of %d\n\r", default_val);
      return default_val;
    }
    for (char c : line) {
      int i = c - '0';
      if (i < 0 || i >= 10) {
        Serial.printf("Please type an _integer_ between %d and %d\n\r", min,
                      max);
        continue;
      }
      ret = (ret * 10) + i;
    }

    // Serial.printf("Got %d\n\r", ret);
    return ret;
  }
}

float prompt_float(String message, float min, float max, float default_val) {
  int32_t val = default_val;
  while (true) {
    Serial.print(message + " [" + default_val + "]: ");

    auto line = read_line();
    // toInt is absolutely stupid and broken, so I guess I'm doing it myself
    float ret = 0;
    if (line.c_str()[0] == 0) {
      Serial.printf("Selected default of %f\n\r", default_val);
      return default_val;
    }

    bool before_point = true;
    int dp = 1;
    for (char c : line) {
      if (c == '.') {
        before_point = false;
        continue;
      }
      if (before_point) {
        int i = c - '0';
        if (i < 0 || i >= 10) {
          Serial.printf(
              "Please type a _floating point number_ between %f and %f\n\r",
              min, max);
          continue;
        }
        ret = (ret * 10) + i;
      } else {
        float i = c - '0';
        ret = ret + (i / pow(10, dp));
        dp++;
      }
    }

    // Serial.printf("Got %d\n\r", ret);
    return ret;
  }
}

#endif
