#include "avr/pgmspace.h"
#include "config.cpp"
#include "key.cpp"
#include "pins_arduino.h"
#include "usb_serial.h"
#include "utils.cpp"

#include <Array.h>

#include "core_pins.h"
#include "elapsedMillis.h"
#include <cstddef>
#include <cstdint>

#ifndef YAHP_CALIBRATE
#define YAHP_CALIBRATE
void blinkblink() {
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_BUILTIN, 1);
    delay(150);
    digitalWrite(LED_BUILTIN, 0);
    delay(250);
  }
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

Array<uint8_t, 16> detect_boards() {
  Serial.println("Getting board info");
  // const size_t LINE_LEN = 256;
  // uint8_t linebuf[LINE_LEN];

  // first, detect which boards are present
  // do this by checking if we can pull up/down
  // the voltage on the lines, or if
  // they automatically return to some given value
  // // for now, just ask \:)
  Serial.print("How many boards are there? [0-255]:");

  uint8_t board_count = prompt_int("How many boards are there?", 0, 255, 1);

  Serial.printf("Setting to %d boards\n\r", board_count);

  Array<uint8_t, 16> boards;

  bool sequential = false;

  sequential =
      confirm("Are they numbered sequentially starting from zero?", true);

  Serial.println();

  if (sequential) {
    for (size_t i = 0; i < board_count; i++) {
      boards.push_back(i);
    }
  } else {
    for (size_t i = 0; i < board_count; i++) {
      uint8_t bid = prompt_int(String("What is the board ID of board number ") +
                                   String(i) + "?",
                               0, 255, i);

      boards.push_back(bid);
    }
  }

  return boards;
}

Array<Array<bool, KEYS_PER_BOARD>, 16> detect_keys(Array<uint8_t, 16> &boards) {

  for (size_t i = 0; i < KEYS_PER_BOARD; i++) {
    pinMode(pins[i], INPUT_PULLDOWN);
  }

  analogReadResolution(10);

  analogReadAveraging(16);

  Serial.println("Detecting keys on boards, using voltage threshold of 0.1v as "
                 "'grounded'");

  float vp = 0.1 / 3.3;
  float thres_f = vp * 1024;
  uint32_t thres = thres_f;

  Array<Array<bool, KEYS_PER_BOARD>, 16> info;

  for (auto &bid : boards) {
    set_board(bid);
    delayMicroseconds(100);

    Array<bool, KEYS_PER_BOARD> bi;

    for (size_t i = 0; i < KEYS_PER_BOARD; i++) {
      delayMicroseconds(30);
      uint32_t val = analogRead(pins[i]);

      if (val > thres) {
        bi.push_back(true);
      } else {
        bi.push_back(false);
      }
    }

    info.push_back(bi);
  }

  Serial.println("Discovered sensor configuration:");

  for (size_t b = 0; b < info.size(); b++) {
    auto &bd = info[b];

    Serial.printf("  (%d)[", b);

    for (size_t s = 0; s < bd.size(); s++) {
      auto present = bd[s];

      if (present) {
        Serial.print("X");
      } else {
        Serial.print("_");
      }
    }

    Serial.print("]");
  }

  for (size_t b = 0; b < info.size(); b++) {
    Serial.printf("b# %d has %d\r\n", b, info[b].size());
  }

  Serial.println();

  return info;
}

key_spec_t detect_range(uint8_t board_num, uint8_t sensor_num, uint8_t midi_num,
                        uint32_t sensor_id) {
  set_board(board_num);
  analogReadResolution(10);
  analogReadAveraging(2);
  delayMicroseconds(100);

  auto pin = pins[sensor_num];

  auto note = format_note(midi_num);

  Serial.println("Please press " + note + " at approximately a PP velocity");
  Serial.println("Release the key fully after the strike");
  delayMicroseconds(50);

  int32_t rest_val = analogRead(pins[sensor_num]);
  Serial.println("Min val: " + String(rest_val));

  int32_t left_hyst = 50;
  Serial.println();

  bool should_configure = confirm("Configure this key?", true);

  if (!should_configure) {
    Serial.println("Nulling key...");
    return key_spec_t(sensor_id, -1, -1, midi_num);
  }

  // wait for the value to rise
  while (true) {
    delayMicroseconds(50);
    uint32_t val = analogRead(pin);
    Serial.print("\r");
    print_value(val, false);

    if (val < (rest_val - left_hyst) || val > (rest_val + left_hyst)) {
      break;
    }
  }

  Serial.print("\r                                                             "
               "                             \n\n\r");

  // now, sample AFAP for 1 second and try to get the highest bound
  auto start = elapsedMillis();

  int32_t min_sense = rest_val;
  int32_t max_sense = rest_val;

  while (elapsedMillis(start) < 3000) {
    int32_t val = analogRead(pin);

    if (val > max_sense) {
      max_sense = val;
    }
    if (val < min_sense) {
      min_sense = val;
    }

    auto delta_min = myabs(rest_val - min_sense);
    auto delta_max = myabs(rest_val - max_sense);
    auto delta_val = myabs(val - rest_val);

    // do an early exit when we know a strike and settle has occurred
    // are we close to the resting point again?
    if (delta_min > 150 && delta_min < 50 && delta_val < 50) {
      break;
    }
    if (delta_max > 150 && delta_min < 50 && delta_val < 50) {
      break;
    }
  }

  auto delta_min = myabs(rest_val - min_sense);
  auto delta_max = myabs(rest_val - max_sense);

  int32_t min;
  int32_t max;

  if (delta_min < delta_max) {
    min = min_sense;
    max = max_sense;
  } else {
    min = max_sense;
    max = min_sense;
  }

  Serial.println("Min:");
  print_value(min, true);
  Serial.println("Max:");
  print_value(max, true);

  Serial.printf("Got minimum value of %d, and maximum value of %d\n\r", min,
                max);
  Serial.flush();

  bool retry = confirm("Do you want to retry?", false);

  Serial.flush();

  if (retry) {
    Serial.println("Retry...");
    return detect_range(board_num, sensor_num, midi_num, sensor_id);
  } else {
    Serial.println("Saving...");
    return key_spec_t(sensor_id, min, max, midi_num);
  }
}

keyboardspec_t detect_ranges(Array<Array<bool, KEYS_PER_BOARD>, 16> &keys,
                             Array<uint8_t, 16> &boards) {

  Array<boardspec_t, NUM_BOARDS> bspecs;
  Array<key_spec_t, KEYS_PER_BOARD> kspecs;

  uint32_t midi_num = FIRST_MIDI_NOTE;

  uint32_t sensor_id = 0;

  for (auto &b : keys) {
    Serial.println("Keys size is " + String(b.size()));
  }

  for (size_t b = 0; b < boards.size(); b++) {
    uint8_t bnum = boards[b];
    auto &board_keys = keys.at(b);
    Serial.println("Looking at board " + String(b));

    Array<sensorspec_t, KEYS_PER_BOARD> sspecs;

    Serial.println("Keys size is " + String(board_keys.size()));
    for (size_t s = 0; s < board_keys.size(); s++) {
      Serial.println("Doing detection for key at " + String(s));
      bool kp = board_keys[s];

      if (!kp) {
        Serial.println("key not present");
        Serial.flush();
        continue;
      }

      // sense this key
      key_spec_t ks = detect_range(bnum, s, midi_num, sensor_id);
      sensorspec_t ss = sensorspec_t(sensor_id, pins[s]);

      midi_num++;
      sensor_id++;

      if (ks.min_val < 0) {
        Serial.printf("Skipping key %d on board %d\r\n", b, s);
        continue;
      }

      kspecs.push_back(ks);
      sspecs.push_back(ss);
    }

    boardspec_t bs(bnum, sspecs);

    bspecs.push_back(bs);
  }

  samplerspec_t s(bspecs);

  keyboardspec_t k(s, kspecs, {});

  return k;
}

float gather_gbl_val(String msg, key_calibration_t &kc, sensorspec_t &ssp) {
  Serial.println(msg);
  Serial.println();
  float latest = 0;
  while (true) {
    if (newline_waiting()) {
      break;
    }

    auto v = analogRead(ssp.pin_num);
    float normal = kc.normalize_sample(v);

    Serial.print("\r");
    for (int i = 0; i < 12; i++) {
      Serial.print("          ");
    }
    Serial.print("\r");
    print_normalized(normal);
    latest = normal;
    delay(30);
  }

  float val = prompt_float(msg, 0, 1, latest);
  Serial.printf("You selected %f\r\n", val);
  bool c = confirm("do you want to continue with this val?", true);
  if (c) {
    return val;
  } else {
    return gather_gbl_val(msg, kc, ssp);
  }
}

global_key_config_t gbl_config(keyboardspec_t &spec) {
  sensorspec_t *sensor = nullptr;
  boardspec_t *board = nullptr;
  key_spec_t *key = nullptr;
  // just pick a key
  for (auto &board_i : spec.sampler.boards) {
    board = &board_i;
    for (auto &sensor_i : board_i.sensors) {
      sensor = &sensor_i;
      goto OUT;
    }
  }
OUT:

  if (sensor == nullptr) {
    Serial.println("No keys were found!");
    return global_key_config_t();
  }

  for (auto &key_i : spec.keys) {
    if (key_i.sensor_id == sensor->sensor_id) {
      key = &key_i;
    }
  }

  if (key == nullptr) {
    Serial.println("No keys were found!");
    return global_key_config_t();
  }

  // now, figure out detect range
  auto kc = key_calibration_t(*key);

  set_board(board->board_num);
  delay(3);

  global_key_config_t gbl;

  gbl.strike =
      gather_gbl_val("What strike point do you want to use?", kc, *sensor);
  gbl.letoff =
      gather_gbl_val("What letoff point do you want to use?", kc, *sensor);
  gbl.repetition =
      gather_gbl_val("What repetition point do you want to use?", kc, *sensor);
  gbl.active =
      gather_gbl_val("What active point do you want to use?", kc, *sensor);
  gbl.damper_down =
      gather_gbl_val("What damper_down point do you want to use?", kc, *sensor);
  gbl.damper_up =
      gather_gbl_val("What damper_up point do you want to use?", kc, *sensor);

  return gbl;
}

keyboardspec_t run_calibration() {
  Serial.println("Beginning calibration");

  auto boards = detect_boards();

  auto keys = detect_keys(boards);

  for (auto &b : keys) {
    Serial.printf("Found count %d\r\n", b.size());
  }

  Serial.println("Calibrating minimum and maximum values");

  // auto specs = detect_ranges({}, {});
  auto specs = detect_ranges(keys, boards);

  specs.gbl = gbl_config(specs);

  return specs;
}

#endif
