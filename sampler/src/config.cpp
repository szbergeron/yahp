#ifndef YAHP_CONFIG
#define YAHP_CONFIG

#include "Array.h"

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

const int PIN_COUNT = 42;

struct key_spec_t {
    uint32_t sensor_id;
};

struct pedal_spec_t {
};

struct sensorspec_t {
    uint8_t pin_num;

    uint32_t sensor_id;
};

struct boardspec_t {
    Array<sensorspec_t, KEYS_PER_BOARD> sensors;
    uint8_t board_num;
};

struct samplerspec_t {
    Array<boardspec_t, NUM_BOARDS> boards;
};

struct keyboardspec_t {
    samplerspec_t sampler;

    Array<key_spec_t, KEY_COUNT_TYPICAL> keys;
    Array<pedal_spec_t, PEDAL_COUNT_TYPICAL> pedals;
};

#endif
