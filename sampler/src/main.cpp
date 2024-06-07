#include "ADC_Module.h"
#include "core_pins.h"
#include "settings_defines.h"
#include "usb_serial.h"
#include "wiring.h"
#include <ADC.h>
#include <ADC_util.h>
#include <Arduino.h>
#include <Array.h>
#include <cstdint>

#define unlikely() ()

void print_value(int value) {
  Serial.print(value);
  //int min = 200;
  //int max = 500;
  value = value > 600 ? 600 : value;
  value = value - 200;
  
  for(int i = 0; i < value; i++) {
    if (i % 1 == 0) {
      Serial.print("\xE2\x96\x88");
    }
  }
  
  Serial.print("\r\n");
}

struct piano_key_t;


ADC *adc = new ADC(); // adc object
const int PIANO_KEY_COUNT = 16;
const int ANALOG_PIN_COUNT = 18;
const int PIN_COUNT = 42;
const int SAMPLING_PIN_COUNT = 16;
const uint8_t INVALID_PIN = 255;
const uint8_t INVALID_KEY = 255;

struct keypair_t {
  uint8_t pin;
  uint8_t key;

  keypair_t(uint8_t pin, uint8_t key) : pin(pin), key(key) {}
  keypair_t() : pin(INVALID_PIN), key(INVALID_KEY) {}
};

const keypair_t INVALID_KEYPAIR = keypair_t(INVALID_PIN, INVALID_KEY);

/*const keypair_t keypairs {
  keypair_t(A)
}*/

// put function declarations here:
int myFunction(int, int);

void errorln(const char *s) { Serial.println(s); }

struct sample_t {
  uint16_t value;
  uint32_t time; // NOTE: this can WRAP! Only use this in comparison with "near"
                 // samples

  sample_t(uint16_t value, uint32_t time) : value(value), time(time) {}

  sample_t() : value(0), time(0) {}
};

struct sample_result_t {
  sample_t sample;
  keypair_t key;

  sample_result_t(keypair_t key, sample_t sample) : sample(sample), key(key) {}
  sample_result_t() : sample(), key() {}
};

const int SAMPLE_BUFFER_LENGTH = 32;
struct sample_buf_t {
  sample_t buffer[SAMPLE_BUFFER_LENGTH];

  // begin points at the newest sample,
  // and (begin + 1) % SAMPLE_BUFFER_LENGTH is the next newest
  uint8_t begin = 0;
  uint8_t size = 0;

  void add_sample(sample_t sample) {
    /*this->begin =
        (this->begin + SAMPLE_BUFFER_LENGTH - 1) % SAMPLE_BUFFER_LENGTH;*/

    this->buffer[this->begin] = sample;
    if (this->size < SAMPLE_BUFFER_LENGTH) {
      this->size++;
    }
  }

  // if n is greater than buffer length,
  // then the oldest still held sample is returned
  // if no samples exist, the sample will be "zero"
  sample_t read_nth_oldest(uint32_t n) {
    auto res = sample_t{0, 0};
    if (this->size == 0) {
      // keep default
    } else {
      res = this->buffer[(n + this->begin) % SAMPLE_BUFFER_LENGTH];
    }

    //print_value(res.value);
    
    return res;
  }
};

enum sample_priority_t {
  SAMPLE_PRIORITY_NEVER,
  SAMPLE_PRIORITY_LOW,
  SAMPLE_PRIORITY_MEDIUM,
  SAMPLE_PRIORITY_HIGH,
};

struct piano_key_t {
  uint16_t no_contact;
  uint16_t bottomed_out;
  uint16_t letoff_rest;
  uint16_t damper_engage;

  uint8_t pin;
  uint8_t key_number;

  sample_priority_t priority =
      SAMPLE_PRIORITY_HIGH; // should do an initial read
  sample_buf_t buf;

  piano_key_t()
      : pin(INVALID_PIN), key_number(INVALID_PIN) { // wrong, but eh, fix later
    //
  }

  piano_key_t(uint8_t pin, uint8_t key_number)
      : pin(pin), key_number(key_number) {
    // restore calibration data?
  }
  
  void print_latest_sample() {
    Serial.print("key: ");
    Serial.println(this->key_number + String(" -- "));
    auto sample = this->buf.read_nth_oldest(0);
    print_value(sample.value);
    print_value(sample.value);
    print_value(sample.value);
    print_value(sample.value);
  }

  void calibrate() {
    // :shrug:
    // all at once?
    // one at a time?
  }

  sample_priority_t add_sample(sample_t sample) {
    this->buf.add_sample(sample);
    
    return SAMPLE_PRIORITY_HIGH;
  }

  keypair_t keypair() { return keypair_t(this->pin, this->key_number); }

  // a lower bound for when we would want the next sample by
  // if this is 0, we want a sample soon with priority
  uint32_t next_sample_due() {
    errorln("bad call to next_sample_due");
    
    return 0;
  }

  // based on current priority
  uint32_t sample_period() {
    // at the moment, just return
    // that we need to read at least 6000 times per second
    auto micros_in_sec = 1000000;
    auto hertz = 6000;
    auto period = micros_in_sec / hertz;

    return period;
  }

  bool sample_due() {
    return true;

    auto now = micros();
    auto last = this->buf.read_nth_oldest(0).time;
    auto since = now - last; // underflow is fine

    if (since > this->sample_period()) {
      return true;
    } else {
      return false;
    }
  }
};

struct piano_keys_t {
  piano_key_t keys[PIANO_KEY_COUNT];

  piano_keys_t() {
    uint8_t pins[16] = {A0, A1, A2, A3,  A4,  A5,  A6,  A7,
                                     A8, A9, A10, A11, A12, A13, A14, A15};

    for (int i = 0; i < PIANO_KEY_COUNT; i++) {
      this->keys[i] = piano_key_t(pins[i], i);
    }
  }
};

piano_keys_t PIANO_KEYS;

ADC_Module *adcs[2];

struct valid_pins_t {
  uint16_t pins[16];
};

struct key_pins_t {};

struct adc_info_t {
  bool allowed_pins[PIN_COUNT];
  ADC_Module *adcm;

  adc_info_t(int idx)
      : allowed_pins{}, adcm(idx == 0 ? adc->adc0 : adc->adc1) {}
};

struct read_pair_t {
  keypair_t pin1;
  keypair_t pin2;

  read_pair_t(keypair_t a, keypair_t b) : pin1(a), pin2(b) {}

  read_pair_t() : pin1(INVALID_KEYPAIR), pin2(INVALID_KEYPAIR) {}
};

struct adcs_info_t {
  adc_info_t adc[2];

  adcs_info_t() : adc{adc_info_t(0), adc_info_t(1)} {}
};

adcs_info_t adcs_info;

// Apparently, this is fast! Like, 1 or 2 microseconds fast
// Faster than a single suboptimally ordered read on an ADC
// I know this seems like a lot of work every time, but
// it actually ends up being worth it to keep
// the ADCs more saturated since they are our primary "bottleneck"
Array<read_pair_t, PIANO_KEY_COUNT>
order_reads(Array<keypair_t, PIANO_KEY_COUNT> pins) {
  //auto start = micros();
  // now that we have most info, we want
  // to split up the pins into a fixed read order
  // that will maximally saturate both ADCs

  Array<keypair_t, ANALOG_PIN_COUNT> assigned_0;
  Array<keypair_t, ANALOG_PIN_COUNT> assigned_1;
  Array<keypair_t, ANALOG_PIN_COUNT> allowed_both;

  // first grab mutually exclusive ones
  for (keypair_t i : pins) {
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

  for (keypair_t pin : allowed_both) {
    if (assigned_0.size() < assigned_1.size()) {
      assigned_0.push_back(pin);
    } else {
      assigned_1.push_back(pin);
    }
  }

  Array<read_pair_t, PIANO_KEY_COUNT> result;

  // NOTE: if the number of pins asked for is odd, one
  // pin will get a free extra read here \:)
  // it's simpler to always have the ADCs paired up,
  // so if we have a space then we need to read something,
  // and a random key will get it!
  // CORRECTION: 255 is a sentinel, don't sample on it
  while (!assigned_0.empty() && !assigned_1.empty()) {
    keypair_t a = assigned_0.back();
    keypair_t b = assigned_1.back();
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

  //auto end = micros();

  //Serial.println("Timings for ordering:");
  //Serial.println(end - start);

  return result;
}

Array<sample_result_t, PIANO_KEY_COUNT>
sample_all(Array<read_pair_t, PIANO_KEY_COUNT> pairs) {
  Array<sample_result_t, PIANO_KEY_COUNT> results;

  for (auto pair : pairs) {
    if (pair.pin1.pin == INVALID_PIN || pair.pin2.pin == INVALID_PIN) {
      keypair_t read;
      if(pair.pin1.pin == INVALID_PIN) {
        read = pair.pin2;
      } else {
        read = pair.pin1;
      }
      
      auto sample = adc->analogRead(read.pin);
      auto now = micros();
      
      if (sample == ADC_ERROR_VALUE) {
        Serial.println("eeeerrror");
        continue;
      }
      
      auto s = sample_result_t(read, sample_t(sample, now));
      
      results.push_back(s);

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

      results.push_back(a);
      results.push_back(b);
    }
  }
  
  return results;
}

void sample_batch() {
  Serial.println("batch");
  auto start = micros();
  Array<keypair_t, PIANO_KEY_COUNT> to_sample;

  for (int i = 0; i < PIANO_KEY_COUNT; i++) {
    auto &key = PIANO_KEYS.keys[i];

    if (key.sample_due()) {
      /*Serial.print("sampling key: ");
      Serial.print(key.pin);
      Serial.print(", ");
      Serial.println(key.key_number);*/
      to_sample.push_back(key.keypair());
    }
  }

  auto read_pairs = order_reads(to_sample);
  
  //Serial.println("Ordered reads:");
  auto sstart = micros();

  auto samples = sample_all(read_pairs);
  
  //Serial.println("Sample round");

  for (auto sample : samples) {
    PIANO_KEYS.keys[sample.key.key].add_sample(sample.sample);
  }
  
  auto s_end = micros();

  // now, do any keys have a note that got played?
  auto end = micros();
  
  Serial.print("Time to sample one round:");
  Serial.println(end - start);
  
  auto mid = micros();
  for(int i = 0; i < 7; i++) {
    PIANO_KEYS.keys[i].print_latest_sample();
  }

  Serial.println("");
  
  end = micros();
  
  Serial.println("Time to print: " + String(mid - start));
  Serial.println("Time in batch: " + String(end - start));
  Serial.println("Time to sample: " + String(s_end - sstart));
}

void test_micros_perf() {
  auto start = micros();

  uint32_t sum = 0;

  for (int i = 0; i < 1000000; i++) {
    sum += micros();
  }

  auto end = micros();

  Serial.println(sum);
  Serial.println("Time to get 1m micros calls:");
  Serial.println(end - start);
}

void setup_adc() {
  //auto start = micros();

  adcs[0] = adc->adc0;
  adcs[1] = adc->adc1;

  for (int i = 0; i <= 1; i++) {
    auto &adci = adcs_info.adc[i];
    auto adcm = adci.adcm;
    //adcm->setAveraging(8);
    adcm->setAveraging(8);
    //adcm->setResolution(12);
    adcm->setSamplingSpeed(ADC_settings::ADC_SAMPLING_SPEED::HIGH_SPEED);
    // adc->checkPin()
    for (int i = A0; i <= A17; i++) {
      adci.allowed_pins[i] = adcm->checkPin(i);
    }
  }
  
  for (auto key: PIANO_KEYS.keys) {
    pinMode(key.pin, INPUT);
  }

  //auto mid = micros();
}

void setup() {
  Serial.begin(38400);

  // put your setup code here, to run once:
  setup_adc();
  
  for(int i = 0; i < 20; i++) {
    Serial.println(i);
    delay(100);
  }
}

void loop() {
  Serial.print("\033[2J\033[H");
  // put your main code here, to run repeatedly:
  // Serial.write(s);
  // test_micros_perf();

  sample_batch();
  delay(6);
}