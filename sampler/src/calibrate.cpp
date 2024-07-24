#include "config.cpp"
#include "utils.cpp"

#include <Array.h>

#include "core_pins.h"
#include "elapsedMillis.h"
#include "pins_arduino.h"
#include "usb_serial.h"
#include "wiring.h"
#include <cstddef>
#include <cstdint>

#ifndef YAHP_CALIBRATE
#define YAHP_CALIBRATE

const uint8_t pins[KEYS_PER_BOARD] = {A0, A1, A2,  A3,  A4,  A5,  A6,  A7,
                                      A8, A9, A10, A11, A12, A13, A16, A17};

const uint8_t FIRST_MIDI_NOTE = 21;

static String format_note(uint8_t index) {
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

static bool confirm(String message, bool default_val) {
  Serial.clear();
  while (true) {

    // auto s = Serial.readStringUntil('\n');
    int s = Serial.read();
    if (s == 'n' || s == 'N') {
      return false;
    } else if (s == 'y' || s == 'Y') {
      return true;
    } else if (s == '\n') {
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

/*static int32_t prompt_int(String message, int32_t min, int32_t max,
                                   int32_t default_val) {
  int32_t val = default_val;
  while (true) {
    // Serial.parseInt()
  }
}*/

static Array<uint8_t, 16> detect_boards() {
  Serial.println("Getting board info");
  // const size_t LINE_LEN = 256;
  // uint8_t linebuf[LINE_LEN];

  // first, detect which boards are present
  // do this by checking if we can pull up/down
  // the voltage on the lines, or if
  // they automatically return to some given value
  // // for now, just ask \:)
  Serial.print("How many boards are there? [0-255]:");

  uint8_t board_count = Serial.parseInt();

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
      Serial.print("What is the board ID (between 0 and 255) of bord number" +
                   String(i) + "? [0-255]:");

      uint8_t bid = Serial.parseInt();

      boards.push_back(bid);
    }
  }

  return boards;
}

static Array<Array<bool, KEYS_PER_BOARD>, 16>
detect_keys(Array<uint8_t, 16> &boards) {

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

  Serial.println();

  return info;
}

static key_spec_t detect_range(uint8_t board_num, uint8_t sensor_num,
                                        uint8_t midi_num, uint32_t sensor_id) {
  set_board(board_num);
  analogReadResolution(10);
  analogReadAveraging(2);
  delayMicroseconds(100);

  auto pin = pins[sensor_num];

  auto note = format_note(midi_num);

  Serial.println("Please press " + note + " at approximately a PP velocity");
  Serial.println("Release the key fully after the strike");
  delayMicroseconds(50);

  int32_t min_val = analogRead(pins[sensor_num]);
  int32_t max_val = min_val;

  int32_t left_hyst = 50;

  // wait for the value to rise
  while (true) {
    delayMicroseconds(50);
    uint32_t val = analogRead(pin);

    if (val < (min_val - left_hyst) || val > (min_val + left_hyst)) {
      break;
    }
  }

  // now, sample AFAP for 1 second and try to get the highest bound
  auto start = elapsedMillis();
  while (elapsedMillis(start) < 1000) {
    int32_t val = analogRead(pin);

    if (val < min_val && val < max_val) {
      max_val = val;
    } else if (val > min_val && val > max_val) {
      max_val = val;
    }
  }

  Serial.printf("Got minimum value of %d, and maximum value of %d\n", min_val,
                max_val);

  bool retry = confirm("Do you want to retry?", false);

  if (retry) {
    return detect_range(board_num, sensor_num, midi_num, sensor_id);
  } else {
    return key_spec_t(sensor_id, min_val, max_val, midi_num);
  }
}

static keyboardspec_t
detect_ranges(Array<Array<bool, KEYS_PER_BOARD>, 16> &keys,
              Array<uint8_t, 16> &boards) {
  Array<boardspec_t, NUM_BOARDS> bspecs;
  Array<key_spec_t, KEYS_PER_BOARD> kspecs;

  uint32_t midi_num = FIRST_MIDI_NOTE;

  uint32_t sensor_id = 0;

  for (size_t b = 0; b < boards.size(); b++) {
    auto &board_keys = keys[b];
    uint8_t bnum = boards[b];

    Array<sensorspec_t, KEYS_PER_BOARD> sspecs;

    for (size_t s = 0; s < keys.size(); s++) {
      bool kp = board_keys[s];

      if (!kp) {
        continue;
      }

      // sense this key
      key_spec_t ks = detect_range(bnum, s, midi_num, sensor_id);
      sensorspec_t ss = sensorspec_t(sensor_id, pins[s]);

      kspecs.push_back(ks);
      sspecs.push_back(ss);

      midi_num++;
      sensor_id++;
    }

    boardspec_t bs(bnum, sspecs);

    bspecs.push_back(bs);
  }

  samplerspec_t s(bspecs);

  keyboardspec_t k(s, kspecs, {});

  return k;
}

static keyboardspec_t run_calibration() {
  Serial.println("Beginning calibration");

  auto boards = detect_boards();

  auto keys = detect_keys(boards);

  Serial.println("Calibrating minimum and maximum values");

  auto specs = detect_ranges(keys, boards);

  return specs;
}

#endif
