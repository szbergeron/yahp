#include "utils.cpp"

#include "Array.h"
#include <ArduinoJson.h>
//#include <ArduinoJson.hpp>
#include "ArduinoJson/Document/JsonDocument.hpp"
#include "ArduinoJson/Json/JsonDeserializer.hpp"
#include "FS.h"
#include "avr/pgmspace.h"
#include <SD.h>
#include <cstddef>

#ifndef YAHP_CONFIG
#define YAHP_CONFIG

const int KEYS_PER_BOARD = 16;
const int NUM_BOARDS = 8;
const int SAMPLE_BUFFER_LENGTH =
    256; // this should capture a dx for even the softest notes
const int SAMPLE_BATCH_TYPICAL_SIZE =
    64; // the maximum typical number of samples within a batch.
        // this can be tuned based on how many notes are
        // going to be due per batch in a reasonable worst-case
        // scenario (requiring an allocation if it exceeds this)
const uint32_t MAX_STALENESS_MICROS = 500;
const uint32_t MIN_STALENESS_MICROS = 200;
const uint32_t KEY_COUNT_TYPICAL = 97;
const uint32_t PEDAL_COUNT_TYPICAL = 4;
const uint32_t CONTRIB_WINDOW = 2;

const int PIN_COUNT = 42;

struct key_spec_t {
  uint32_t sensor_id = 0;

  // the value observed by the sensor when...
  float max_val = 0; // the hammer is at its highest position when playing a PP note
  float min_val = 0; // the hammer is at rest

  // each of these is normalized to a 0-1
  // position of both `self` and the neighboring
  // sensors
  // some non-linear factor? is it needed?
  Array<float, CONTRIB_WINDOW> factor_left;
  Array<float, CONTRIB_WINDOW> factor_right;

  uint8_t midi_note = 0;
  uint8_t midi_channel = 0;

  key_spec_t(uint32_t sid, float min_val, float max_val, uint8_t midi_note)
      : sensor_id(sid), max_val(max_val), min_val(min_val),
        midi_note(midi_note) {}

  key_spec_t() {}
};

struct global_key_config_t {
  // values between 0 and 1, with 1 being the max value,
  // and 0 being the min value according to the key_spec

  float active = 0.1;

  // the point where the jack moves off
  // of the knuckle and the hammer
  // begins flying freely
  float letoff = 0.85;

  // the point where the hammer
  // makes contact with the "string".
  // note that this is not the max value,
  // since the hammer can actually bounce
  // a bit below the max value when playing
  // softer than the calibration strike.
  // this aims to be where the hammer
  // first makes contact with the striker rail
  float strike = 0.95;

  // the point where the jack comes back
  // onto the knuckle, and the key can be played again
  float repetition = 0.75;

  // damper_up is the point where
  // the damper should lift off of the string
  //
  // this is separate from damper_down,
  // as there is some amount of slop in real actions
  // around the dampers _and_ this prevents
  // a storm of note_on/note_off being sent
  // because of sensor noise
  float damper_up = 0.4;
  float damper_down = 0.3;

  float max_velocity;
  float min_velocity;

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
};

struct pedal_spec_t {
  uint32_t sensor_id;

  float max_val;
  float min_val;

  pedal_spec_t() {}
};

struct sensorspec_t {
  uint8_t pin_num = 0;

  uint32_t sensor_id = 0;

  sensorspec_t(uint32_t sensor_id, uint8_t pin_num)
      : pin_num(pin_num), sensor_id(sensor_id) {}

  sensorspec_t() {}
};

struct boardspec_t {
  Array<sensorspec_t, KEYS_PER_BOARD> sensors;
  uint8_t board_num = 0;

  boardspec_t(uint8_t board_num, Array<sensorspec_t, KEYS_PER_BOARD> sensors)
      : sensors(sensors), board_num(board_num) {}

  boardspec_t() {}
};

struct samplerspec_t {
  Array<boardspec_t, NUM_BOARDS> boards;

  samplerspec_t(Array<boardspec_t, NUM_BOARDS> boards) : boards(boards) {}
};

const size_t JSON_FILE_MAX_LENGTH = 128000;
EXTMEM char JSON_FILE[JSON_FILE_MAX_LENGTH];

struct keyboardspec_t {
  samplerspec_t sampler;

  Array<key_spec_t, KEY_COUNT_TYPICAL> keys;
  Array<pedal_spec_t, PEDAL_COUNT_TYPICAL> pedals;

  global_key_config_t gbl;

  keyboardspec_t(samplerspec_t sampler,
                 Array<key_spec_t, KEY_COUNT_TYPICAL> keys,
                 Array<pedal_spec_t, PEDAL_COUNT_TYPICAL> pedals)
      : sampler(sampler), keys(keys), pedals(pedals) {}
};

// I KNOW this is horrible and has memory safety issues.
// I just am not dealing with that right here, right now
keyboardspec_t* from_sd() {
    if (!SD.begin(BUILTIN_SDCARD)) {
        Serial.println("Couldn't init SD");
        return nullptr;
    }

    if (!SD.exists("config.json")) {
        Serial.println("No config on SD");
        return nullptr;
    }

    auto f = SD.open("config.json", FILE_READ);

    size_t file_size = 0;

    file_size = f.readBytes(JSON_FILE, JSON_FILE_MAX_LENGTH);

    JSON_FILE[file_size] = 0; // null term

    JsonDocument doc;
    //deserializeJson(doc, JSON_FILE);
}

#endif
