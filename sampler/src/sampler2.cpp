#ifndef YAHP_SAMPLER
#define YAHP_SAMPLER

#include "ADC.h"
#include "ADC_Module.h"
#include "Array.h"
#include "config.cpp"
#include "core_pins.h"
#include "utils.cpp"

struct sample_t {

  uint16_t value;
  uint32_t time; // NOTE: this can WRAP! Only use this in comparison with "near"
                 // samples

  sample_t(uint16_t value, uint32_t time) : value(value), time(time) {}

  sample_t() : value(0), time(0) {}
};

struct sample_buf_t {
  sample_t buffer[SAMPLE_BUFFER_LENGTH];

  // begin points at the newest sample,
  // and (begin + 1) % SAMPLE_BUFFER_LENGTH is the next newest
  uint32_t begin = 0;
  uint32_t size = 0;

  uint32_t unackd;

  void add_sample(sample_t sample) {
    /*this->begin =
        (this->begin + SAMPLE_BUFFER_LENGTH - 1) % SAMPLE_BUFFER_LENGTH;*/
    this->begin++;
    this->begin %= SAMPLE_BUFFER_LENGTH;

    this->unackd += 1;

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

  inline sample_t latest() { return this->read_nth_oldest(0); }
};

struct sensor_t;

struct sample_request_t {
  sensor_t *sensor;
  uint8_t pin;

  sample_request_t() : sensor(nullptr), pin(0) {}

  sample_request_t(uint8_t pin, sensor_t *sensor) : sensor(sensor), pin(pin) {}
};

struct sensor_t {
  enum class sensor_mode_e {
    KEY,
    PEDAL,
    FADER,
  };

  enum class poll_priority_e {
    SLOW, // the slowest of the bunch, means
          // we can poll "rarely" in computer timescales
          // indicates events that want to happen within
          // "window of present" at human timescales,
          // but is not
    RELAXED,
    CRITICAL,
  };

  uint32_t sensor_id;

  sample_buf_t buf;
  poll_priority_e priority = poll_priority_e::RELAXED;

  uint8_t pin;

  // key should be immediately sampled
  bool due_now() {
    uint32_t now = micros();

    switch (this->priority) {
    case poll_priority_e::SLOW:
      // TODO
      return true;
    case poll_priority_e::RELAXED:
      if (now - this->buf.latest().time > MAX_STALENESS_MICROS) {
        return true;
      }
    case poll_priority_e::CRITICAL:
      return true; // AFAP, but why are you asking me?
    }
  }

  // key should be sampled _if convenient_
  bool due_relaxed() {
    uint32_t now = micros();

    if (now - this->buf.latest().time > MIN_STALENESS_MICROS) {
      return true;
    } else {
      return false;
    }
  }

  sample_request_t make_sample_request() {
    return sample_request_t(this->pin, this);
  }

  sensor_t(uint8_t pin, uint32_t sensor_id) : sensor_id(sensor_id), pin(pin) {
    //
  }
};

struct board_t {
  Array<sensor_t, KEYS_PER_BOARD> keys;
  uint8_t board_num;

  void do_round() {
    // check if we even need to sample anything
    bool some_due = false;

    for (auto &k : this->keys) {
      if (k.due_now()) {
        some_due = true;
      }
    }

    if (!some_due) {
      // move on asap
      return;
    }

    // start setting the pins _now_
    // so we have as much time as possible
    // before needing to sample

    Array<sample_request_t, KEYS_PER_BOARD> requests;

    for (auto &k : this->keys) {
      if (k.due_now() || k.due_relaxed()) {
        auto r = k.make_sample_request();
        requests.push_back(r);
      }
    }
  }

  void sample_all(Array<sample_request_t, KEYS_PER_BOARD> &requests) {
    Array<sample_request_t, KEYS_PER_BOARD> for_a;
    Array<sample_request_t, KEYS_PER_BOARD> for_b;
    Array<sample_request_t, KEYS_PER_BOARD> for_both;
  }

  board_t(uint8_t board_num, Array<sensorspec_t, KEYS_PER_BOARD> &keysc)
      : board_num(board_num) {
    for (auto &keyc : keysc) {
      sensor_t sensor(keyc.pin_num, keyc.sensor_id);
      this->keys.push_back(sensor);
    }
  }
};

struct adc_info_t {
  bool allowed_pins[PIN_COUNT];
  ADC_Module *adcm;

  adc_info_t(int idx, ADC *adc)
      : allowed_pins{}, adcm(idx == 0 ? adc->adc0 : adc->adc1) {}

  adc_info_t() : allowed_pins{}, adcm(nullptr) {}
};

struct adcs_info_t {
  adc_info_t adc[2];

  adcs_info_t(ADC *adcp) : adc{adc_info_t(0, adcp), adc_info_t(1, adcp)} {}

  adcs_info_t() : adc{} {}
};

struct sampler_t {
  ADC adc;
  Array<board_t, NUM_BOARDS> boards;

  sampler_t(ADC adc_, samplerspec_t &spec) : adc(adc_) {
    for (auto &boardc : this->boards) {
        board_t b(boardc);
        this->boards.push_back(b);
    }
  }

  void sample_round() {
  }
};

#endif
