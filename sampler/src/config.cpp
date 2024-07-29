// #include "ArduinoJson/Json/PrettyJsonSerializer.hpp"
#include "WProgram.h"
#include "utils.cpp"
#include "magnets.cpp"

#include "Array.h"

#include "ArduinoJson/Json/PrettyJsonSerializer.hpp"
#include "ArduinoJson/Variant/JsonVariantConst.hpp"
#include <ArduinoJson.hpp>

#include "FS.h"
#include "wiring.h"
#include <SD.h>
#include <cstddef>

using namespace ArduinoJson;

#ifndef YAHP_CONFIG
#define YAHP_CONFIG

const int KEYS_PER_BOARD = 16;
const int NUM_BOARDS = 8;
const int SAMPLE_BUFFER_LENGTH =
    16; // this should capture a dx for even the softest notes
const int SAMPLE_BATCH_TYPICAL_SIZE =
    64; // the maximum typical number of samples within a batch.
        // this can be tuned based on how many notes are
        // going to be due per batch in a reasonable worst-case
        // scenario (requiring an allocation if it exceeds this)
const uint32_t MAX_STALENESS_MICROS = 500;
const uint32_t MIN_STALENESS_MICROS = 200;
const uint32_t KEY_COUNT_MAX = 97;
const uint32_t PEDAL_COUNT_MAX = 4;
const uint32_t CONTRIB_WINDOW = 2;

const uint8_t pins[KEYS_PER_BOARD] = {A3,  A2,  A1, A0, A7,  A6,  A5,  A4,
                                      A11, A10, A9, A8, A17, A16, A13, A12};

const int PIN_COUNT = 42;

struct key_spec_t {
  uint32_t sensor_id = 0;

  // the value observed by the sensor when...
  //
  // the hammer is at its highest position when playing a PP note
  float max_val = 0;
  // the hammer is at rest
  float min_val = 0;

#ifdef USE_CONTRIB_WINDOW
  // each of these is normalized to a 0-1
  // position of both `self` and the neighboring
  // sensors
  // some non-linear factor? is it needed?
  Array<float, CONTRIB_WINDOW> factor_left;
  Array<float, CONTRIB_WINDOW> factor_right;
#endif

  uint8_t midi_note = 0;
  uint8_t midi_channel = 0;

  interpolater_t<MAGNET_INTERPOLATOR_POINTS> curve;

  key_spec_t() {}

  key_spec_t(uint32_t sid, float min_val, float max_val, uint8_t midi_note)
      : sensor_id(sid), max_val(max_val), min_val(min_val),
        midi_note(midi_note) {}

  key_spec_t(JsonObject j)
      : sensor_id(j["sensor_id"]), max_val(j["max_val"]), min_val(j["min_val"]),
        midi_note(j["midi_note"]), midi_channel(j["midi_channel"]) {}

  JsonDocument to_json() {
    JsonDocument d;

    d["sensor_id"] = this->sensor_id;
    d["max_val"] = this->max_val;
    d["min_val"] = this->min_val;
    d["midi_note"] = this->midi_note;
    d["midi_channel"] = this->midi_channel;

    return d;
  }
};

struct global_key_config_t {
  // values between 0 and 1, with 1 being the max value,
  // and 0 being the min value according to the key_spec

  float active = 0.05;

  // the point where the jack moves off
  // of the knuckle and the hammer
  // begins flying freely
  float letoff = 0.75;

  // the point where the hammer
  // makes contact with the "string".
  // note that this is not the max value,
  // since the hammer can actually bounce
  // a bit below the max value when playing
  // softer than the calibration strike.
  // this aims to be where the hammer
  // first makes contact with the striker rail
  float strike = 0.85;

  // the point where the jack comes back
  // onto the knuckle, and the key can be played again
  float repetition = 0.35;

  // damper_up is the point where
  // the damper should lift off of the string
  //
  // this is separate from damper_down,
  // as there is some amount of slop in real actions
  // around the dampers _and_ this prevents
  // a storm of note_on/note_off being sent
  // because of sensor noise
  float damper_up = 0.2;
  float damper_down = 0.1;

  float max_velocity = 0.29;
  float min_velocity = 0;

  // just...flat?
  float bezier_p1x = 0;
  float bezier_p1y = 0;
  float bezier_p2x = 0.82;
  float bezier_p2y = 0.526;
  float bezier_p3x = 0.728;
  float bezier_p3y = 1.137;
  float bezier_p4x = 1;
  float bezier_p4y = 1;

  global_key_config_t() {}

  global_key_config_t(JsonObject j)
      : active(j["active"]), letoff(j["letoff"]), strike(j["strike"]),
        repetition(j["repetition"]), damper_up(j["damper_up"]),
        damper_down(j["damper_down"]), max_velocity(j["max_velocity"]),
        min_velocity(j["min_velocity"]) {}

  JsonDocument to_json() {
    JsonDocument d;

    d["active"] = this->active;
    d["letoff"] = this->letoff;
    d["strike"] = this->strike;
    d["repetition"] = this->repetition;
    d["damper_up"] = this->damper_up;
    d["damper_down"] = this->damper_down;
    d["max_velocity"] = this->max_velocity;
    d["min_velocity"] = this->min_velocity;

    return d;
  }
};

struct pedal_spec_t {
  uint32_t sensor_id;

  float max_val;
  float min_val;

  pedal_spec_t() {}

  pedal_spec_t(JsonObject j)
      : sensor_id(j["sensor_id"]), max_val(j["max_val"]),
        min_val(j["min_val"]) {}

  JsonDocument to_json() {
    JsonDocument d;

    d["sensor_id"] = this->sensor_id;
    d["max_val"] = this->max_val;

    return d;
  }
};

struct sensorspec_t {
  uint8_t pin_num = 0;

  uint32_t sensor_id = 0;

  sensorspec_t() {}

  sensorspec_t(uint32_t sensor_id, uint8_t pin_num)
      : pin_num(pin_num), sensor_id(sensor_id) {}

  sensorspec_t(JsonObject j)
      : pin_num(j["pin_num"]), sensor_id(j["sensor_id"]) {}

  JsonDocument to_json() {
    JsonDocument d;

    d["sensor_id"] = this->sensor_id;
    d["pin_num"] = this->pin_num;

    return d;
  }
};

struct boardspec_t {
  Array<sensorspec_t, KEYS_PER_BOARD> sensors;
  uint8_t board_num = 0;

  boardspec_t() {}

  boardspec_t(uint8_t board_num, Array<sensorspec_t, KEYS_PER_BOARD> sensors)
      : sensors(sensors), board_num(board_num) {}

  boardspec_t(JsonObject j) : board_num(j["board_num"]) {
    auto ja = j["sensors"].as<JsonArray>();

    for (auto ent : ja) {
      this->sensors.push_back(sensorspec_t(ent.as<JsonObject>()));
    }
  }

  JsonDocument to_json() {
    JsonDocument d;

    d["board_num"] = this->board_num;

    JsonDocument di;

    auto a = di.to<JsonArray>();

    for (auto &sensor : this->sensors) {
      a.add(sensor.to_json());
    }

    d["sensors"] = di;

    return d;
  }
};

struct samplerspec_t {
  Array<boardspec_t, NUM_BOARDS> boards;

  samplerspec_t(Array<boardspec_t, NUM_BOARDS> boards) : boards(boards) {}

  samplerspec_t() {}

  samplerspec_t(JsonObject j) {
    auto ja = j["boards"].as<JsonArray>();

    for (auto ent : ja) {
      boardspec_t b(ent.as<JsonObject>());
      this->boards.push_back(b);
    }
  }

  JsonDocument to_json() {
    JsonDocument di;

    auto a = di.to<JsonArray>();

    for (auto &board : this->boards) {
      a.add(board.to_json());
    }

    JsonDocument d;
    d["boards"] = di;

    return d;
  }
};
// static uint8_t *JSON_FILE = nullptr;

struct keyboardspec_t {
  samplerspec_t sampler;

  Array<key_spec_t, KEY_COUNT_MAX> keys;
  Array<pedal_spec_t, PEDAL_COUNT_MAX> pedals;

  global_key_config_t gbl;

  keyboardspec_t() {}

  keyboardspec_t(samplerspec_t sampler, Array<key_spec_t, KEY_COUNT_MAX> keys,
                 Array<pedal_spec_t, PEDAL_COUNT_MAX> pedals)
      : sampler(sampler), keys(keys), pedals(pedals) {}

  keyboardspec_t(JsonObject d)
      : sampler(d["sampler"].as<JsonObject>()),
        gbl(d["global"].as<JsonObject>()) {
    JsonArray kja = d["keys"].as<JsonArray>();
    JsonArray pja = d["pedals"].as<JsonArray>();

    for (auto ent : kja) {
      key_spec_t ks(ent.as<JsonObject>());
      this->keys.push_back(ks);
    }

    for (auto ent : pja) {
      pedal_spec_t ps(ent.as<JsonObject>());
      this->pedals.push_back(ps);
    }
  }

  JsonDocument to_json() {
    JsonDocument d;

    JsonDocument kj;
    JsonDocument pj;

    JsonArray kja = kj.to<JsonArray>();
    JsonArray pja = pj.to<JsonArray>();

    for (auto &key : this->keys) {
      auto jd = key.to_json();
      kja.add(jd);
    }

    for (auto &pedal : this->pedals) {
      auto jd = pedal.to_json();
      pja.add(jd);
    }

    d["keys"] = kja;
    d["pedals"] = pja;
    d["sampler"] = this->sampler.to_json();
    d["global"] = this->gbl.to_json();

    return d;
  }
};

static uint8_t KBSPEC_BUF[sizeof(keyboardspec_t)];
static keyboardspec_t *KBSPEC = nullptr;

// I KNOW this is horrible and has memory safety issues.
// I just am not dealing with that right here, right now
// NOTE: when this is called, it will destroy any other keyboardspec
// that it has returned prior! it only maintains one backing buffer
static keyboardspec_t *yahp_from_sd() {
  const size_t JSON_FILE_MAX_LENGTH = 32000;
  char JSON_FILE[JSON_FILE_MAX_LENGTH];

  if (!SD.exists("config.json")) {
    Serial.println("No config on SD");
    return nullptr;
  }

  auto f = SD.open("config.json", FILE_READ);

  size_t file_size = 0;

  file_size = f.readBytes(JSON_FILE, JSON_FILE_MAX_LENGTH);

  JSON_FILE[file_size] = 0; // null term

  f.close();

  JsonDocument doc;
  deserializeJson(doc, JSON_FILE);

  if (KBSPEC != nullptr) {
    KBSPEC->~keyboardspec_t();
  }

  keyboardspec_t *kbds = new (KBSPEC_BUF) keyboardspec_t(doc.as<JsonObject>());
  KBSPEC = kbds;

  // auto p = new (SPEC_ALLOC) keyboardspec_t(std::move(kbds));

  return kbds;
}

static void yahp_to_sd(keyboardspec_t &kbs) {
  Serial.println("Saving config to sd...");
  String buf;

  // size_t len = 0;
  // serializeJson();
  // size_t len = serializeJsonPretty(d, &JSON_FILE, JSON_FILE_MAX_LENGTH - 1);
  // size_t len = serializeJsonPretty(d, buf, 28000);
  {
    auto d = kbs.to_json();
    size_t len = serializeJsonPretty(d, buf);

    auto f = SD.open("config.json", FILE_WRITE_BEGIN);
  }

  f.write(&buf[0], len);
  f.close();
  Serial.println("Done!");
}

#endif
