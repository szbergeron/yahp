#include "Array.h"
#include "core_pins.h"
#include "utils.cpp"

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

  inline sample_t latest() { return this->read_nth_oldest(0); }
};

struct sensor_t;

struct sample_request_t {
  sensor_t &sensor;
  uint8_t pin;
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

  poll_priority_e priority = poll_priority_e::RELAXED;

  // sensor_mode_e sensor_mode;
  sample_buf_t buf;

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
    // TODO
  }
};

struct board_t {
  Array<sensor_t, KEYS_PER_BOARD> keys;

  void sample_round() {
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

    //
  }
};

struct sampler_t {
  Array<board_t, NUM_BOARDS> boards;

  void sample_round() {
    for (auto &board : this->boards) {
    }
  }
};
