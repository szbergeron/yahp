#include "ADC_Module.h"
#include "core_pins.h"
#include "settings_defines.h"
#include "usb_serial.h"
#include "wiring.h"
#include <ADC.h>
#include <ADC_util.h>
#include <Arduino.h>
#include <Array.h>
#include <math.h>
#include <cstdint>
#include "config.cpp"
#include "calibrate.cpp"
#include "sample.cpp"

#define USB_MIDI_SERIAL
#define COUNTERS_ENABLED false
#define PROFILE_ENABLED false

#if defined(USB_MIDI_SERIAL)
#define ENABLE_MIDI
#endif

#if defined(ENABLE_MIDI)
  #include <MIDIUSB.h>
#endif

uint32_t counter = 0;

void wait(uint32_t prior, uint32_t later) {
    while(micros() - prior < later) {
        // do nothing
    }
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

void errorln(const char *s) { Serial.println(s); }


struct sample_result_t {
  sample_t sample;
  keypair_t key;

  sample_result_t(keypair_t key, sample_t sample) : sample(sample), key(key) {}
  sample_result_t() : sample(), key() {}
};

enum sample_priority_t {
  SAMPLE_PRIORITY_NEVER,
  SAMPLE_PRIORITY_LOW,
  SAMPLE_PRIORITY_MEDIUM,
  SAMPLE_PRIORITY_HIGH,
};

enum NOTE_STATE {
  // No note is active,
  NOTE_OFF,
  
  // additional state for damping?
  
  // A note is currently playing
  // undamped
  NOTE_ON,
};

enum KEY_STATE {
  // the "laziest" state, no interaction
  // is known to be active,
  // poll rate is safe to be reduced
  KEY_RESTING,

  // _some_ interaction is occurring,
  // but the key is not necessarily
  // within the critical zone,
  KEY_READY,
  
  // Key has reached between letoff and strike,
  // highest priority sampling for accurate velocity here
  KEY_CRITICAL,
  
  KEY_STRIKING,
  
  // additional states for repetition lever?
};

volatile uint32_t CANARY_VALUE = 123456;

uint16_t DEFAULT_BOTTOM = 270;
uint16_t DEFAULT_UP = 460;
uint16_t DEFAULT_GAP = DEFAULT_UP - DEFAULT_BOTTOM;

struct key_calibration_t {
  volatile uint32_t canary = CANARY_VALUE;

  uint16_t letoff_th_on = DEFAULT_BOTTOM + 25;
  uint16_t letoff_th_off = DEFAULT_BOTTOM + 30;
  
  uint16_t strike_th_on = DEFAULT_BOTTOM + 5;
  uint16_t strike_th_off = DEFAULT_BOTTOM + 10;
  
  uint16_t ready_th_on = DEFAULT_UP - 55;
  uint16_t ready_th_off = DEFAULT_UP - 50;
  
  // bigger gap here just 'cuz
  uint16_t damper_on_th_on = DEFAULT_UP - 60;
  uint16_t damper_on_th_off = DEFAULT_UP - 90;

  int16_t resting_value = 500;

  key_calibration_t(uint32_t bottom, uint32_t top) {
      this->letoff_th_on = bottom + 35;
      this->letoff_th_off = bottom + 40;
      
      this->strike_th_on = bottom + 5;
      this->strike_th_off = bottom + 10;
      
      this->ready_th_on = top - 55;
      this->ready_th_off = top - 50;
      
      // bigger gap here just 'cuz
      this->damper_on_th_on = top - 60;
      this->damper_on_th_off = top - 90;
  }
  
  key_calibration_t() {
    if(this->canary != CANARY_VALUE) {
      errorln("even bad during construction!");
    }
  }
};

//uint32_t tops[16] = {501, 498, 489};
//uint32_t bottoms[16] = {340, 345, 348};
uint32_t tops[16] = {748};
uint32_t bottoms[16] = {300};

struct piano_key_t {
  key_calibration_t calibration;
  KEY_STATE keystate = KEY_RESTING;
  NOTE_STATE notestate = NOTE_OFF;

  //uint16_t no_contact;
  //uint16_t bottomed_out;
  //uint16_t letoff_rest;
  //uint16_t damper_engage;

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
    this->calibration = key_calibration_t(bottoms[key_number], tops[key_number]);
    // restore calibration data?
  }
  
  // this is where we handle state transitions
  // and queue up event dispatches
  // 
  void process_samples() {
    if (this->calibration.canary != CANARY_VALUE) {
      errorln("canary mismatch!");
      delay(10000);
    }
    auto latest = this->buf.read_nth_oldest(0);
    bool note_played = false;
    bool damper_on = false;
    bool damper_off = false;
    
    switch (this->keystate) {
      case KEY_RESTING:
        if (latest.value < this->calibration.ready_th_on) {
          Serial.println("KEY ACTIVATES");
          
          // TODO: lift damper
          this->keystate = KEY_READY;
        }
        break;
      case KEY_READY:
        if (latest.value > this->calibration.damper_on_th_on) {
          damper_on = true;
        } else if (latest.value < this->calibration.damper_on_th_off) {
          damper_off = true;
        }
        if (latest.value > this->calibration.ready_th_off) {
          Serial.println("Goes back to resting...");
          this->keystate = KEY_RESTING;
        } else if (latest.value < this->calibration.letoff_th_on) {
          // passed letoff, now in critical
          this->keystate = KEY_CRITICAL;
          Serial.println("letoff lets go");
        } else if(latest.value > this->calibration.ready_th_off) {
          Serial.println("key up");
          this->keystate = KEY_RESTING;
        }
        break; // no fallthrough since we want at least two samples within CRITICAL before STRIKE
      case KEY_CRITICAL:
        if (latest.value < this->calibration.strike_th_on) {
          Serial.println("STRIKE");
          //Serial.println("critical got note!");
          //Serial.println(latest.value);
          //Serial.println(this->calibration.strike_th_on);
          // when the magic happens
          note_played = true;
          this->keystate = KEY_STRIKING;
        } else if (latest.value > this->calibration.letoff_th_off) {
          // note abandoned, but keep it active
          Serial.println("note abandoned");
          this->keystate = KEY_READY;
        }
        break;
      case KEY_STRIKING:
        if (latest.value > this->calibration.letoff_th_off) {
          Serial.println("letoff resets");
          // outside of CRITICAL, so allow new note to play
          this->keystate = KEY_READY;
        }
    }
    
    if(note_played) {
      this->strike_velocity(0);
    }
    
    if(damper_on) {
      this->damp(0);
    }
    
    if(damper_off) {
      this->undamp();
    }

    if(damper_on && damper_off) {
        errorln("bro");
        delay(1000);
    }
  }
  
  void undamp() {

    if(this->notestate == NOTE_ON) {
      // do nothing if already undamped
      return;
    }
    
    this->notestate = NOTE_ON;

    Serial.println("Undamp...");
    // TODO: figure out how to send a damper off for an unplayed key?
    return;
    
    #if defined(ENABLE_MIDI)
    usbMIDI.sendAfterTouchPoly(70 + this->key_number, 1, 0);
    #endif
  }
  
  void damp(uint32_t n) {
    if(this->notestate == NOTE_OFF) {
      // do nothing if already damped
      return;
    }
    
    this->notestate = NOTE_OFF;

    Serial.println("Damper back on");
    #if defined(ENABLE_MIDI)
      usbMIDI.sendNoteOff(70 + this->key_number, 127, 0);
      //usbMIDI.send_now();
    #endif
  }
  
  float linear_regression(Array<sample_t, SAMPLE_BUFFER_LENGTH> points) {
    // subtract time of the first point, and value of the minimum
    float min_x = points.back().time;
    
    float min_val = 1000;
    for(auto point: points) {
      if(point.value < min_val) {
        min_val = point.value;
      }
    }

    float sum_x = 0;
    float sum_y = 0;
    float sum_xmy = 0;
    float sum_xmx = 0;
    float n = points.size();
    for(auto point: points) {
      float x = (point.time - min_x);
      float y = (point.value - min_val);
      sum_x += x;
      sum_y += y;
      sum_xmy += x * y;
      sum_xmx += x * x;
    }
    
    float m_x = sum_x / n;
    float m_y = sum_y / n;
    
    //Serial.println("Means: " + String(m_x) + ", " + String(m_y));
    
    float SS_xy = sum_xmy - (n * m_y * m_x);
    float SS_xx = sum_xmx - (n * m_x * m_x);
    
    float b_1 = SS_xy / SS_xx;
    float b_0 = m_y - b_1 * m_x;
    
    return b_1 * -10000;
  }
  
  // calculate the velocity of a strike
  // precondition: a strike is (and should be) triggered
  // n is the sample to start from, marking the "transient"
  void strike_velocity(uint32_t n) {
    Array<sample_t, SAMPLE_BUFFER_LENGTH> samples;

    uint32_t mins = UINT32_MAX;
    uint32_t maxs = 0;
    
    for(int i = n; i < SAMPLE_BUFFER_LENGTH; i++) {
      auto sample = this->buf.read_nth_oldest(i);

      if (sample.value > this->calibration.letoff_th_on) {
        // don't include it -- either noise or too old
        // TODO: break out if multiple out of range
      } else if(sample.value < this->calibration.strike_th_on) {
        // these values can be suspect, as they occur "at the bottom"
        // and tend to flatten out. Excluding these as well as those
        // from before letoff means we get a very clean reading of the
        // unobstructed, unimpeded movement of the hammer
        // right before "hitting the string"
      } else {
        samples.push_back(sample);
        mins = min(sample.value, mins);
        maxs = max(sample.value, maxs);
      }
    }
    
    //Serial.println("Strike detected!");

    float velocity = linear_regression(samples);
    // we want to curve map this since out of the box it's dumb

    //float new_velocity = pow(velocity, 1) * 7 - 8;
    float new_velocity = velocity / 5;
    
    int first_time = 0;
    if(!samples.empty()) {
      first_time = samples.back().time;
    }

    Serial.println("Velocity: " + String(new_velocity));
    Serial.println("Strike profile:");
    Serial.printf("Threshold for KEY_CRITICAL: %d\r\n", this->calibration.letoff_th_on);
    Serial.printf("Threshold for KEY_STRIKING: %d\r\n", this->calibration.strike_th_on);
    Serial.println("Max observed: " + String(maxs));
    Serial.println("Min observed: " + String(mins));


    if(PROFILE_ENABLED) {
        //float points = min((float)samples.size(), 32.0);
        //float gap = max((float)samples.size() / 32.0, 1.0);
        for(int i = samples.size() - 1; i >= 0; i-= 1) {
          Serial.printf("%10d -- ", samples[i].time);
          Serial.printf("%5d -- ", samples[i].time - first_time);
          print_value(samples[i].value);
        }
        /*for(auto& sample: samples) {
          //Serial.print(sample.time);
        }*/
    }

    Serial.println("Velocity: " + String(new_velocity));
    print_bounds(new_velocity, 250, 0);
    
    uint32_t velocity_i = new_velocity;
    if (velocity_i > 127) {
      velocity_i = 127;
    } else if (velocity_i < 0) {
      velocity_i = 0;
    }
    
    #if defined(ENABLE_MIDI)
      usbMIDI.sendNoteOn(70 + this->key_number, velocity_i, 0);
      //usbMIDI.send_now();
    #endif
    
    this->notestate = NOTE_ON;
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
    this->last_sample = sample.time;
    
    return SAMPLE_PRIORITY_HIGH;
  }

  keypair_t keypair() { return keypair_t(this->pin, this->key_number); }

  // based on current priority
  uint32_t sample_period() {
    // at the moment, just return
    // that we need to read at least 6000 times per second
    auto micros_in_sec = 1000000;
    auto hertz = 6000;
    auto period = micros_in_sec / hertz;

    return period;
  }

  uint32_t last_sample = 0;

  bool sample_due(uint32_t now) {
    auto gap = now - this->last_sample;
    auto resting_period = 500;

    if (gap < resting_period && this->keystate == KEY_RESTING) {
        // sample at least once per millisecond
        return false;
    } else {
        return true;
    }

    /*auto now = micros();
    auto last = this->buf.read_nth_oldest(0).time;
    auto since = now - last; // underflow is fine

    if (since > this->sample_period()) {
      return true;
    } else {
      return false;
    }*/
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
    if (pair.pin1.pin == INVALID_PIN || pair.pin2.pin == INVALID_PIN) [[unlikely]] {
      keypair_t read;
      if(pair.pin1.pin == INVALID_PIN) {
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
  auto ts_a = micros();
  Array<keypair_t, PIANO_KEY_COUNT> to_sample;

  uint32_t now = micros();

  for (int i = 0; i < PIANO_KEY_COUNT; i++) {
    auto &key = PIANO_KEYS.keys[i];

    if (key.sample_due(now)) {
      to_sample.push_back(key.keypair());
    }
  }

  if(counter % 1000 == 0 && COUNTERS_ENABLED) {
      Serial.println("Number of keys to sample:" + String(to_sample.size()));
  }

  auto ts_b = micros();

  auto read_pairs = order_reads(to_sample);

  auto ts_c = micros();
  
  auto samples = sample_all(read_pairs);

  auto ts_d = micros();

  for (auto sample : samples) {
    PIANO_KEYS.keys[sample.key.key].add_sample(sample.sample);
  }

  auto ts_e = micros();
  
  // do processing
  for (auto& key: PIANO_KEYS.keys) {
    key.process_samples();
  }

  auto ts_f = micros();

  // now, do any keys have a note that got played?
  auto end = micros();
  
  //Serial.print("Time to sample one round:");
  //Serial.println(end - start);
  
  auto mid = micros();
  for(int i = 0; i <= -1; i++) {
    PIANO_KEYS.keys[i].print_latest_sample();
  }

  auto ts_g = micros();

  //Serial.println("");
  
  end = micros();

  if(false) {
      gap(ts_a, ts_b, "dues");
      gap(ts_b, ts_c, "order");
      gap(ts_c, ts_d, "sample");
      gap(ts_d, ts_e, "add");
      gap(ts_e, ts_f, "process");
      gap(ts_f, ts_g, "print");
      gap(ts_a, end, "total");
      delay(10);
  }
  
  //Serial.println("Time to print: " + String(mid - start));
  //Serial.println("Time in batch: " + String(end - start));
  //Serial.println("Time to sample: " + String(s_end - sstart));
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
    adcm->setAveraging(2);
    //adcm->setResolution(12);
    adcm->setSamplingSpeed(ADC_settings::ADC_SAMPLING_SPEED::VERY_HIGH_SPEED);
    // adc->checkPin()
    for (int i = A0; i <= A17; i++) {
      adci.allowed_pins[i] = adcm->checkPin(i);
    }
  }
  
  for (auto& key: PIANO_KEYS.keys) {
    pinMode(key.pin, INPUT);
  }

  for(int i = 1; i < 16; i++) {
      auto& k = PIANO_KEYS.keys[i];
      pinMode(k.pin, INPUT_PULLUP);
      tops[i] = 300;
      bottoms[i] = 0;

      delay(10);

      auto v = analogRead(k.pin);
      print_value(v);
  }

  //auto mid = micros();
}

void setup() {
  Serial.begin(38400);

  // put your setup code here, to run once:
  setup_adc();
  
  for(int i = 0; i < 20; i++) {
    Serial.println(i);
  }

  for(auto& k: PIANO_KEYS.keys) {
      k = piano_key_t(k.pin, k.key_number);
  }
}

uint32_t last = 0;
uint32_t ts_c = 0;

void loop() {
  //Serial.print("\033[2J\033[H");
  // put your main code here, to run repeatedly:
  // Serial.write(s);
  // test_micros_perf();

  if(counter++ % 1000 == 0 && COUNTERS_ENABLED) {
      auto ts_cn = micros();

      gap(ts_c, ts_cn, "per 1000");

      ts_c = ts_cn;
  }

  auto now = micros();
  last = now;

  auto ts_a = micros();
  for(int i = 0; i <= 0; i++) {
    // for each bit
    for(int b = 0; b < 3; b++) {
        uint8_t bit = (i >> b) & 1;
        digitalWrite(b, bit);
    }
    sample_batch();
  }
  auto ts_b = micros();


  //gap(ts_a, ts_b, "gap");
}
