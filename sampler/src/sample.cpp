#include "core_pins.h"
#include "utils.cpp"
#include <ADC.h>
#include <ADC_util.h>
#include <Arduino.h>
#include <Array.h>
#include <cstdint>
#include <math.h>

const int SAMPLE_BUFFER_LENGTH =
    256; // this should capture a dx for even the softest notes
const int SAMPLE_BATCH_TYPICAL_SIZE =
    64; // the maximum typical number of samples within a batch.
        // this can be tuned based on how many notes are
        // going to be due per batch in a reasonable worst-case
        // scenario (requiring an allocation if it exceeds this)
const uint16_t INVALID_KEY = -1;
const uint8_t INVALID_PIN = 255;
const uint8_t INVALID_BOARD = 255;

const uint8_t KEYS_PER_BOARD = 16;
const int ANALOG_PIN_COUNT = 18;
const int PIN_COUNT = 42;

struct sample_request_t {
  uint8_t board;
  uint8_t pin;

  uint16_t key_id;

  sample_request_t(uint16_t key_id, uint8_t board, uint8_t selection)
      : key_id(key_id), pin(selection), board(board) {}

  sample_request_t(): board(INVALID_BOARD), pin(INVALID_PIN), key_id(INVALID_KEY) {}
};

struct sample_t {

  uint16_t value;
  uint32_t time; // NOTE: this can WRAP! Only use this in comparison with "near"
                 // samples

  sample_t(uint16_t value, uint32_t time) : value(value), time(time) {}

  // "big" value since smaller ones are more active
  sample_t() : value(1000), time(0) {}
};

struct sample_result_t {
  sample_request_t for_request;
  sample_t val;

  sample_result_t(sample_request_t sample_of, sample_t sample): for_request(sample_of), val(sample) {}
};

struct sample_buf_t {
  sample_t buffer[SAMPLE_BUFFER_LENGTH];

  // begin points at the newest sample,
  // and (begin + 1) % SAMPLE_BUFFER_LENGTH is the next newest
  uint32_t begin = 0;
  uint32_t size = 0;

  void add_sample(sample_t sample) {
    /*this->begin =
        (this->begin + SAMPLE_BUFFER_LENGTH - 1) % SAMPLE_BUFFER_LENGTH;*/
    this->begin++;
    this->begin %= SAMPLE_BUFFER_LENGTH;

    this->buffer[this->begin] = sample;
    if (this->size < SAMPLE_BUFFER_LENGTH) {
      this->size++;
    }
  }

  // if n is greater than buffer length,
  // then the oldest still held sample is returned
  // if no samples exist, the sample will be "zero"
  sample_t read_nth_oldest(uint32_t n) {
    // Serial.println("N is: " + String(n));
    //  clamp n
    if (n > SAMPLE_BUFFER_LENGTH) [[unlikely]] {
      n = SAMPLE_BUFFER_LENGTH;
    }

    auto res = sample_t{0, 0};
    if (this->size == 0) {
      // keep default
    } else {
      auto start = this->begin;
      auto with_room = start + SAMPLE_BUFFER_LENGTH;
      auto offsetted = with_room - n;
      auto idx = offsetted % SAMPLE_BUFFER_LENGTH;
      // Serial.println("Returns idx: " + String(idx));
      res = this->buffer[idx];
      // res = this->buffer[(n + this->begin) % SAMPLE_BUFFER_LENGTH];
    }

    // print_value(res.value);

    return res;
  }
};

ADC *adc = new ADC(); // adc object

struct adc_info_t {
  bool allowed_pins[PIN_COUNT];
  ADC_Module *adcm;

  adc_info_t(int idx)
      : allowed_pins{}, adcm(idx == 0 ? adc->adc0 : adc->adc1) {}
};

struct adcs_info_t {
  adc_info_t adc[2];

  adcs_info_t() : adc{adc_info_t(0), adc_info_t(1)} {}
};

adcs_info_t adcs_info;

sample_request_t INVALID_KEYPAIR =
    sample_request_t(INVALID_KEY, INVALID_BOARD, INVALID_PIN);

struct read_pair_t {
  sample_request_t pin1;
  sample_request_t pin2;

  read_pair_t(sample_request_t a, sample_request_t b) : pin1(a), pin2(b) {}

  read_pair_t() : pin1(INVALID_KEYPAIR), pin2(INVALID_KEYPAIR) {}
};

struct sampler {
  Array<sample_result_t, SAMPLE_BATCH_TYPICAL_SIZE>
  sample_batch(Array<sample_request_t, SAMPLE_BATCH_TYPICAL_SIZE> batch) {
    // first, we want to group by board since we can only
    // parallelize reads within a board
    // why bubble sort? well, this array is probably already
    // sorted by board in almost all situations. bubble sort is
    // just really fast in those scenarios, and optimizes well
    // (with good branch prediction even without simd, and we have bp/spec-ex on
    // teensy 4!)

    // TODO: sort by board

    Array<sample_result_t, SAMPLE_BATCH_TYPICAL_SIZE> result;

    uint8_t current_board = 0;
    Array<sample_request_t, 16> board_batch;

    for (auto &r : batch) {
      if (r.board != current_board) {
        // execute on the current available batch
        this->set_board(current_board);

        this->sample_in_board(result, board_batch);
      }
    }
  }

  void set_board(uint8_t board) {
    // I did an oopsie and numbered the pins backwards,
    // so lets "fix it in post"

    for (uint_fast8_t v = 0; v < 8; v++) {
      // range is from pin 14 down to 7
      uint8_t pin = 14 - v;
      uint8_t val = (board >> v) & 0x1;

      digitalWriteFast(0, 0);
      digitalWriteFaster(val, pin);
    }
  }

  Array<read_pair_t, KEYS_PER_BOARD>
  order_reads(const Array<sample_request_t, KEYS_PER_BOARD> &pins) {
    // auto start = micros();
    //  now that we have most info, we want
    //  to split up the pins into a fixed read order
    //  that will maximally saturate both ADCs

    Array<sample_request_t, KEYS_PER_BOARD> assigned_0;
    Array<sample_request_t, KEYS_PER_BOARD> assigned_1;
    Array<sample_request_t, KEYS_PER_BOARD> allowed_both;

    // first grab mutually exclusive ones
    for (sample_request_t i : pins) {
      bool allowed_0i = adcs_info.adc[0].allowed_pins[i.pin];
      bool allowed_1i = adcs_info.adc[1].allowed_pins[i.pin];

      if (allowed_0i && allowed_1i) {
        // allowed for both, can't yet assign greedily
        allowed_both.push_back(i);
      } else if (allowed_0i) {
        // exclusively allowed for adc0, so can greedily push
        assigned_0.push_back(i);
      } else if (allowed_1i) {
        // same for adc1
        assigned_1.push_back(i);
      } else {
        // ???
        errorln("an assigned pin wasn't valid on either adc for sampling");
      }
    }

    for (sample_request_t pin : allowed_both) {
      if (assigned_0.size() < assigned_1.size()) {
        assigned_0.push_back(pin);
      } else {
        assigned_1.push_back(pin);
      }
    }

    Array<read_pair_t, KEYS_PER_BOARD> result;

    // NOTE: if the number of pins asked for is odd, one
    // pin will get a free extra read here \:)
    // it's simpler to always have the ADCs paired up,
    // so if we have a space then we need to read something,
    // and a random key will get it!
    // CORRECTION: 255 is a sentinel, don't sample on it
    while (!assigned_0.empty() && !assigned_1.empty()) {
      sample_request_t a = assigned_0.back();
      sample_request_t b = assigned_1.back();
      assigned_0.pop_back();
      assigned_1.pop_back();

      read_pair_t pair = read_pair_t(a, b);

      result.push_back(pair);
    }

    if (!assigned_0.empty()) {
      result.push_back(read_pair_t(assigned_0.back(), INVALID_KEYPAIR));
      assigned_0.pop_back();
    }

    if (!assigned_1.empty()) {
      result.push_back(read_pair_t(INVALID_KEYPAIR, assigned_1.back()));
      assigned_1.pop_back();
    }

    // auto end = micros();

    // Serial.println("Timings for ordering:");
    // Serial.println(end - start);

    return result;
  }

  void sample_in_board(Array<sample_result_t, SAMPLE_BATCH_TYPICAL_SIZE> &out,
                       const Array<sample_request_t, 16> &in) {
    auto ordered_reads = this->order_reads(in);

    //Array<sample_result_t, KEYS_PER_BOARD> results;

    for (auto pair : ordered_reads) {
      if (pair.pin1.pin == INVALID_PIN || pair.pin2.pin == INVALID_PIN)
          [[unlikely]] {
        sample_request_t read;
        if (pair.pin1.pin == INVALID_PIN) {
          read = pair.pin2;
        } else {
          read = pair.pin1;
        }

        auto sample = adc->analogRead(read.pin);
        auto now = micros();

        if (sample == ADC_ERROR_VALUE) [[unlikely]] {
          Serial.println("eeeerrror");
          continue;
        }

        auto s = sample_result_t(read, sample_t(sample, now));

        out.push_back(s);

      } else {
        auto res = adc->analogSynchronizedRead(pair.pin1.pin, pair.pin2.pin);
        auto now = micros();

        if (res.result_adc0 == ADC_ERROR_VALUE) [[unlikely]] {
          Serial.print("pin read was invalid:");
          Serial.print(pair.pin1.pin);
          Serial.print(", ");
          Serial.println(pair.pin2.pin);
          // error("pin read was invalid while sampling");
          continue;
        }

        auto a = sample_result_t(pair.pin1, sample_t(res.result_adc0, now));
        auto b = sample_result_t(pair.pin2, sample_t(res.result_adc1, now));

        out.push_back(a);
        out.push_back(b);
      }
    }
  }
};
