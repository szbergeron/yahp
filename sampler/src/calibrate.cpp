#include "avr/pgmspace.h"
#include "config.cpp"
#include "key.cpp"
#include "keyboard.cpp"
#include "magnets.cpp"
#include "pins_arduino.h"
#include "usb_serial.h"
#include "utils.cpp"

#include "ArduinoJson/Document/JsonDocument.hpp"
#include "ArduinoJson/Object/JsonObject.hpp"

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

tempboards_t detect_boards() {
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

  vector_t<uint8_t, NUM_BOARDS> boards;

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

  return {boards};
}

vector_t<vector_t<bool, KEYS_PER_BOARD>, NUM_BOARDS>
detect_keys(tempboards_t &boards) {

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

  vector_t<vector_t<bool, KEYS_PER_BOARD>, NUM_BOARDS> info;

  for (auto &bid : boards.boards) {
    set_board(bid);
    delayMicroseconds(100);

    vector_t<bool, KEYS_PER_BOARD> bi;

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

/*key_spec_t detect_range(uint8_t board_num, uint8_t sensor_num, uint8_t
midi_num, uint32_t sensor_id) { set_board(board_num); analogReadResolution(10);
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
}*/

/*keyboardspec_t
detect_ranges(vector_t<vector_t<bool, KEYS_PER_BOARD>, NUM_BOARDS> &keys,
              tempboards_t &boards) {

  vector_t<boardspec_t, NUM_BOARDS> bspecs;
  vector_t<key_spec_t, KEYS_PER_BOARD> kspecs;

  uint32_t midi_num = FIRST_MIDI_NOTE;

  uint32_t sensor_id = 0;

  for (auto &b : keys) {
    Serial.println("Keys size is " + String(b.size()));
  }

  for (size_t b = 0; b < boards.boards.size(); b++) {
    uint8_t bnum = boards.boards[b];
    auto &board_keys = keys.at(b);
    Serial.println("Looking at board " + String(b));

    vector_t<sensorspec_t, KEYS_PER_BOARD> sspecs;

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
}*/

float gather_gbl_val(String msg, key_calibration_t &kc, sensorspec_t &ssp) {
  Serial.println(msg);
  Serial.println();
  float latest = 0;
  while (true) {
    if (newline_waiting()) {
      break;
    }

    auto v = analogRead(ssp.pin_num);
    float normal = kc.map(v);

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
  Serial.println("making global");
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
  auto kc = key_calibration_t(*key, *sensor);

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

/*tempboards_t detect_boards2() {
    for(uint8_t b = 0; b <= 255; b++) {
        set_board(b);
        delayMicroseconds(10);

        // first, send a pulse out to bring output high
        auto pin = pins[0];

        pinMode(pin, OUTPUT);
        digitalWrite(uint8_t pin, uint8_t val);

    }
}*/

tempkey_t select_key(tempboards_t &boards) {
  uint16_t baselines[NUM_BOARDS][KEYS_PER_BOARD];
  for (size_t b = 0; b < NUM_BOARDS; b++) {
    for (size_t k = 0; k < KEYS_PER_BOARD; k++) {
      baselines[b][k] = 0x9FFF;
    }
  }

  for (size_t b = 0; b < boards.boards.size(); b++) {
    auto bnum = boards.boards[b];
    set_board(bnum);

    for (size_t knum = 0; knum < KEYS_PER_BOARD; knum++) {
      auto pin = pins[knum];
      baselines[b][knum] = analogRead(pin);
    }
  }

  Serial.println("Please press the key you want to select");

  while (true) {
    for (size_t b = 0; b < boards.boards.size(); b++) {
      auto bnum = boards.boards[b];
      set_board(bnum);
      delayMicroseconds(5);

      for (size_t knum = 0; knum < KEYS_PER_BOARD; knum++) {
        auto pin = pins[knum];
        uint16_t baseline = baselines[b][knum];
        uint16_t val = analogRead(pin);
        if (val > baseline + 50) {
          Serial.printf(
              "baseline was %d, val found was %d, on bnum %d pin %d\r\n",
              (int)baseline, (int)val, (int)bnum, (int)pin);
          return tempkey_t(bnum, pin);
        }
      }
    }
  }
}

keyboardspec_t key_calibration() {
  auto boards = detect_boards();

  samplerspec_t samplerspec;
  vector_t<key_spec_t, KEY_COUNT_MAX> keys;
  vector_t<pedal_spec_t, PEDAL_COUNT_MAX> pedals;

  uint32_t start_midi =
      prompt_int("What is the lowest midi note on your piano? (Usually the "
                 "minimum on an 88 key keyboard is 21)",
                 0, 127, FIRST_MIDI_NOTE + 30);

  uint32_t midi_channel = prompt_int(
      "What midi channel should notes play on by default?", 0, 16, 1);

  uint32_t num_keys =
      prompt_int("How many keys does your piano have?", 0, 127 - start_midi,
                 min(88, 127 - (int32_t)start_midi));

  uint32_t csid = 0;
  for (uint32_t midi_num = start_midi; midi_num < start_midi + num_keys;
       midi_num++) {
    auto n = format_note(midi_num);
    Serial.printf("Configuring note %s (midi id %d)\r\n", n.c_str(),
                  (int)midi_num);
    bool in_use = confirm("Configure this key?", true);
    if (!in_use) {
      continue;
    }

    Serial.printf("Please press %s on your piano (will be sid %d)\r\n",
                  n.c_str(), csid);
    auto sensor = select_key(boards);

    //

    Serial.printf("Key %s is assigned pin %d on board %d\r\n", n.c_str(),
                  (int)sensor.pin, (int)sensor.board);

    auto interpolater = drop_test(sensor.board, sensor.pin);

    uint32_t sid = csid++;

    sensorspec_t sensorspec(sid, sensor.pin, interpolater.points);

    samplerspec.get_board(sensor.board).sensors.push_back(sensorspec);
    key_spec_t k(sid, midi_num, midi_channel);
    keys.push_back(k);
  }

  while (confirm("Do you have an additional pedal to add?", false)) {
    Serial.println("Not implemented");
  }

  return keyboardspec_t(samplerspec, keys, pedals);
}

float read_key(uint32_t sid, keyboard_t *kbd) {
  return kbd->sampler->find_sensor(sid)->buf.latest().height;
}

// Doesn't free itself, reboot after done!
keyboard_t *make_kbd(fullspec_t spec) {
  ADC adc;
  auto sampler = new sampler_t(adc, spec.keyboard.sampler);
  auto kbd = new keyboard_t(spec, sampler);

  return kbd;
}

JsonDocument configure_offsets(fullspec_t &spec) {
  confirm("Hands off the keyboard! Hit enter once understood", true);
  delay(500);

  const uint32_t AVERAGES_COUNT = 500;

  auto kbd = make_kbd(spec);

  for (uint32_t averages = 0; averages < AVERAGES_COUNT; averages++) {
    /*for (auto &key : spec.keyboard.keys) {
        //
    }*/
    //
    kbd->sampler->sample_round();
    for (kbd_key_t &key : kbd->keys) {
      float cur_val = read_key(key.sensor->sensor_id, kbd);

      if (averages == 0) {
        key.calibration.spec.bound_min = cur_val;
        key.calibration.spec.bound_max = cur_val;
      } else {
        key.calibration.spec.bound_min =
            (key.calibration.spec.bound_min * averages + cur_val) /
            (averages + 1);
      }
    }
  }

  Serial.println("Now, play each note at PPP level (or lift the hammer just "
                 "barely to touch the striker rail!)");
  Serial.println("Press enter when done");

  while (!newline_waiting()) {
    kbd->sampler->sample_round();
    for (kbd_key_t &key : kbd->keys) {
      float cur_val = read_key(key.sensor->sensor_id, kbd);

      key.calibration.spec.bound_max =
          max(key.calibration.spec.bound_max, cur_val);
    }
  }

  if (confirm("Do you want to use the new range config generated here?",
              true)) {
    JsonDocument jd;
    for (kbd_key_t &key : kbd->keys) {
      JsonDocument jdi;

      jdi["min"] = key.calibration.spec.bound_min;
      jdi["max"] = key.calibration.spec.bound_max;

      jd[key.sensor->sensor_id] = jdi;
      //
    }

    return jd;
  } else {
    // just reboot
    doReboot();
    JsonDocument never;
    return never;
  }
}

const size_t JSON_FILE_MAX_LENGTH = 32000;
char JSON_FILE[JSON_FILE_MAX_LENGTH];

result_t<JsonDocument *, unit_t> json_from_sd(const char *name) {
  Serial.println("Grabbing json from sd");

  if (!SD.exists(name)) {
    Serial.println("No config on SD");
    return result_t<JsonDocument *, unit_t>::err({});
  }

  auto f = SD.open(name, FILE_READ);

  size_t file_size = 0;

  file_size = f.readBytes(JSON_FILE, JSON_FILE_MAX_LENGTH);

  JSON_FILE[file_size] = 0; // null term

  f.close();

  Serial.println("done loading file");

  JsonDocument *doc = new JsonDocument;
  deserializeJson(*doc, JSON_FILE);

  Serial.println("deserialized");

  return result_t<JsonDocument *, unit_t>::ok(doc);
}

void json_to_sd(const char *name, JsonDocument doc) {
  Serial.println("Saving config to sd...");
  String buf;
  size_t len = serializeJsonPretty(doc, buf);
  auto f = SD.open(name, FILE_WRITE_BEGIN);
  f.write(&buf[0], len);
  f.close();
}

static fullspec_t get_spec() {
  keyboardspec_t s;
  {
    auto r = json_from_sd("calibration.json");
    if (r.is_ok()) {
      auto d = r.unwrap();
      Serial.println("Loaded calibration json file");
      // we already have sampler then
      s = keyboardspec_t((*d).as<JsonObject>());
      delete d;
    } else {
      s = key_calibration();
      json_to_sd("calibration.json", s.to_json());

      // come back through now
      // with a fresh heap, and sane everything
      doReboot();
    }
  }

  Serial.println("Loaded calibration data");

  global_key_config_t gbl;
  {
    auto r2 = json_from_sd("global.json");
    if (r2.is_ok()) {
      auto d = r2.unwrap();
      // return samplerspec_t(r.unwrap().as<JsonObject>());
      gbl = global_key_config_t((*d).as<JsonObject>());
      delete d;
    } else {
      gbl = gbl_config(s);
      json_to_sd("global.json", gbl.to_json());

      // fresh heap, uniform flow
      // (always want to return based on value of load)
      doReboot();
    }
  }

  auto spec = fullspec_t(s, gbl);

  {
    auto r = json_from_sd("offsets.json");
    if (r.is_ok()) {
      JsonDocument *d = r.unwrap();
      JsonObject o = (*d).as<JsonObject>();
      for (key_spec_t &key : spec.keyboard.keys) {
        if (o.containsKey(String(key.sensor_id))) {
          JsonObject ko = o[String(key.sensor_id)];
          key.bound_min = ko["min"];
          key.bound_max = ko["max"];
        }
      }
      delete d;
    } else {
      auto jd = configure_offsets(spec);
      json_to_sd("offsets.json", jd);
      doReboot();
    }
  }

  return spec;
}

#endif
