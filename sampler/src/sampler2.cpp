#include "config.cpp"
#include "utils.cpp"

#include "ADC.h"
#include "ADC_Module.h"
#include "Array.h"
#include "core_pins.h"

#ifndef YAHP_SAMPLER
#define YAHP_SAMPLER

// it seems that a typical conversion takes about 4.92-ish
// microseconds when set to averaging(0) and the fastest
// conversion speed
//
// this gives us ~29000 cycles at stock clocks,
// which should give us some good compute during idle!
// it also means we get about 49 microseconds per
// sample of even a 20 key range (if we aren't split
// across boards weirdly, and can run both adcs at
// full saturation), meaning 20 samples per millisecond
// of even a "pretty severe" load scenario across
// all "READY" keys therein
static const uint32_t CONVERSION_TIME_NS = 49000;

__attribute__((always_inline)) static inline void step_idle(uint32_t window_ns,
                                                            bool run_all);

struct sample_t {

  uint16_t value;
  uint32_t time;
  //uint16_t time_l;
  //uint16_t time_h; // NOTE: this can WRAP! Only use this in comparison with "near" samples

  /*uint32_t time() {
      //uint32_t h = this->time_h;
      //uint32_t l = this->time_l;
      //return (h << 16) + l;
  }*/

  sample_t(uint16_t value, uint32_t time) : value(value), time(time) {}

  sample_t() : value(0), time(0) {}
};


template <uint32_t BUFSIZE = SAMPLE_BUFFER_LENGTH> struct sample_buf_t {
  sample_t buffer[BUFSIZE];

  // begin points at the newest sample,
  // and (begin + 1) % SAMPLE_BUFFER_LENGTH is the next newest
  uint32_t begin = 0;
  uint32_t size = 0;

  uint32_t unackd;

  void add_sample(sample_t sample) {
    /*this->begin =
        (this->begin + SAMPLE_BUFFER_LENGTH - 1) % SAMPLE_BUFFER_LENGTH;*/
    this->begin++;
    this->begin %= BUFSIZE;

    this->unackd += 1;

    this->buffer[this->begin] = sample;
    if (this->size < BUFSIZE) {
      this->size++;
    }
  }

  void clear() {
    this->begin = 0;
    this->size = 0;
    this->unackd = 0;
  }

  // if n is greater than buffer length,
  // then the oldest still held sample is returned
  // if no samples exist, the sample will be "zero"
  sample_t read_nth_oldest(uint32_t n) {
    // Serial.println("N is: " + String(n));
    //  clamp n
    if (n > BUFSIZE) [[unlikely]] {
      n = BUFSIZE;
    }

    auto res = sample_t{0, 0};
    if (this->size == 0) {
      // keep default
    } else {
      auto start = this->begin;
      auto with_room = start + BUFSIZE;
      auto offsetted = with_room - n;
      auto idx = offsetted % BUFSIZE;
      // Serial.println("Returns idx: " + String(idx));
      res = this->buffer[idx];
      // res = this->buffer[(n + this->begin) % SAMPLE_BUFFER_LENGTH];
    }

    // print_value(res.value);

    return res;
  }

  inline sample_t latest() { return this->read_nth_oldest(0); }
};

/*char (*__kaboom)[sizeof( sample_buf_t<45056> )] = 1;
void kaboom_print( void )
{
    printf( "%d", __kaboom );
}*/

struct sensor_t;

struct sample_request_t {
  sensor_t *sensor;
  uint8_t pin;

  sample_request_t() : sensor(nullptr), pin(0) {}

  // if this was default constructed, this will return false
  bool is_real() { return this->sensor != nullptr; }

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

  uint32_t sensor_id = 0;

  sample_buf_t<SAMPLE_BUFFER_LENGTH> buf;
  poll_priority_e priority = poll_priority_e::RELAXED;

  uint8_t pin = 0;

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
      } else {
        return false;
      }
    case poll_priority_e::CRITICAL:
      return true; // AFAP, but why are you asking me?
    default:
      [[unlikely]] return false;
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

  void add_sample(sample_t s) { this->buf.add_sample(s); }

  sensor_t(uint8_t pin, uint32_t sensor_id) : sensor_id(sensor_id), pin(pin) {
    //
  }

  sensor_t() {
      //errorln("sensor_t was default constructed");
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

struct board_t {
  Array<sensor_t, KEYS_PER_BOARD> keys;
  uint8_t board_num;
  adcs_info_t adcs;

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

    this->sample_all(requests);
  }

  void sample_all(Array<sample_request_t, KEYS_PER_BOARD> &requests) {
    Array<sample_request_t, KEYS_PER_BOARD> for_a;
    Array<sample_request_t, KEYS_PER_BOARD> for_b;
    Array<sample_request_t, KEYS_PER_BOARD> for_both;

    for (auto request : requests) {
      bool allowed_0i = this->adcs.adc[0].allowed_pins[request.pin];
      bool allowed_1i = this->adcs.adc[1].allowed_pins[request.pin];

      if (allowed_0i && allowed_1i) {
        // allowed for both, can't yet assign greedily
        for_both.push_back(request);
      } else if (allowed_0i) {
        // exclusively allowed for adc0, so can greedily push
        for_a.push_back(request);
      } else if (allowed_1i) {
        // same for adc1
        for_b.push_back(request);
      } else {
        // ???
        errorln("an assigned pin wasn't valid on either adc for sampling");
      }
    }

    sample_request_t a;
    sample_request_t b;

    while (true) {
      if (!for_a.empty()) {
        a = for_a.back();
        for_a.pop_back();
      } else if (!for_both.empty()) {
        a = for_both.back();
        for_both.pop_back();
      }

      if (!for_b.empty()) {
        b = for_b.back();
        for_b.pop_back();
      } else if (!for_both.empty()) {
        b = for_both.back();
        for_both.pop_back();
      }

      uint32_t ts_a;
      uint32_t ts_b;

      auto &adc_a = this->adcs.adc[0].adcm;
      auto &adc_b = this->adcs.adc[1].adcm;

      if (a.is_real()) {
#ifdef YAHP_DEBUG
        bool v = adc_a->checkPin(a.pin);
        if (!v) {
          eloop(String("adc 0 (a) tried to read disallowed pin: ") + a.pin);
        }
#endif
        adc_a->startReadFast(a.pin);
        ts_a = micros();
        ;
      }

      if (b.is_real()) {
#ifdef YAHP_DEBUG
        bool v = adc_b->checkPin(b.pin);
        if (!v) {
          eloop(String("adc 1 (b) tried to read disallowed pin: ") + b.pin);
        }
#endif
        adc_b->startReadFast(b.pin);
        ts_b = micros();
        ;
      }

      // do one big-budget run
      step_idle(CONVERSION_TIME_NS, false);

      while (true) {
        bool a_done = true;
        bool b_done = true;

        // is either one done?
        if (a.is_real() && !adc_a->isComplete()) {
          a_done = false;
        }
        if (b.is_real() && !adc_b->isComplete()) {
          b_done = false;
        }

        // track when _both_ are complete
        // so that we avoid having to do individual stalls below,
        // should reduce code size in this loop
        if (a_done && b_done) {
          break;
        }

        // if not, at the moment we want to avoid
        // too much complexity and the gains from calling
        // step_once _again_ with so little time remaining
        // would probably be negligible
      }

      // get values
      if (a.is_real()) {
        auto v_a = adc_a->readSingle();
        sample_t s(v_a, ts_a);
        a.sensor->add_sample(s);
      }

      if (b.is_real()) {
        auto v_b = adc_b->readSingle();
        sample_t s(v_b, ts_b);
        a.sensor->add_sample(s);
      }
    }
  }

  board_t(boardspec_t &bspec, adcs_info_t &adcs_info)
      : board_num(bspec.board_num), adcs(adcs_info) {
    for (auto &keyc : bspec.sensors) {
      sensor_t sensor(keyc.pin_num, keyc.sensor_id);
      this->keys.push_back(sensor);
    }
  }

  board_t() : board_num(0), adcs() {}
};

struct sampler_t {
  ADC *adc = nullptr;
  Array<board_t, NUM_BOARDS> boards;

  sensor_t *find_sensor(uint32_t sensor_id) {
    for (auto &board : this->boards) {
      for (auto &sensor : board.keys) {
        if (sensor.sensor_id == sensor_id) {
          return &sensor;
        }
      }
    }

    return nullptr;
  }

  sampler_t(ADC adc_, samplerspec_t &spec) : adc(new ADC(adc_)) {
    for (auto boardc : spec.boards) {
      adcs_info_t info(this->adc);
      board_t b(boardc, info);
      this->boards.push_back(b);
    }
  }

  ~sampler_t() { delete this->adc; }

  sampler_t() {}

  void sample_round() {
    for (auto &board : this->boards) {
      board.do_round();
    }
  }
};

#endif
