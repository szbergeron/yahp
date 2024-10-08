#include "core_pins.h"
#include "sampler2.cpp"
#include "utils.cpp"

#include "usb_midi.h"
#include "usb_serial.h"
#include <cmath>

#ifndef YAHP_KEY
#define YAHP_KEY

#define ENABLE_MIDI

static const size_t STRIKE_BUF_SIZE = 48;
static const uint32_t SETTLE_DELAY_MS = 100;

struct key_calibration_t {
  key_spec_t spec;

  // all of these are normalized to
  // ideally fall within a 0-1 range,
  // though values outside of that
  // range are also completely allowed

  /*inline float normalize_sample(uint16_t sample) {
    float as_f = sample;

    float normalized =
        (as_f - this->spec.min_val) / (this->spec.max_val - this->spec.min_val);

    return normalized;
  }*/

  inline float map(float height) {
    // the sensor has some calibration, but we also have some
    // variation vertically that is easier to calibrate, so
    // we adjust the "pseudo-normalized" output from the interpolater
    // to match the actual range of the hammer
    float range = this->spec.bound_max - this->spec.bound_min;

    float val = (height - this->spec.bound_min) / range;

#ifdef YAHP_DEBUG
    if (val > 2) [[unlikely]] {
      Serial.printf("weird? %f, %f, %f, %f\r\n", val, height,
                    this->spec.bound_min, this->spec.bound_max);
    }
#endif
    // in ideal circumstances, this should return a value that falls
    // within [0, 1] and is, much more importantly, _consistent_ from key to key

    return val;
  }

  //

  /*inline void verify_states(global_key_config_t &gbl) {
    if (gbl.active > gbl.damper_down) {
      Serial.printf("active %f is greater than dd %f\n", gbl.active,
                    gbl.damper_down);
    }

    if (gbl.damper_down > gbl.damper_up) {
      Serial.printf("dd %f is greater than du %f\n", gbl.active,
                    gbl.damper_down);
    }

    if (gbl.damper_up > gbl.strike) {
      Serial.printf("du %f is greater than strike %f\n", gbl.active,
                    gbl.damper_down);
    }

    if (gbl.active > gbl.repetition) {
      Serial.printf("active %f is greater than repetition %f\n", gbl.active,
                    gbl.damper_down);
    }

    if (gbl.repetition > gbl.letoff) {
      Serial.printf("repetition %f is greater than letoff %f\n", gbl.active,
                    gbl.damper_down);
    }

    if (gbl.letoff > gbl.strike) {
      Serial.printf("letoff %f is greater than strike %f\n", gbl.active,
                    gbl.damper_down);
    }
  }*/

  key_calibration_t(key_spec_t &spec, sensorspec_t &sspec) : spec(spec) {}
};

struct kbd_key_t {
  enum class key_state_e {
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

  enum class damper_state_e {
    // No note is active,
    DAMPER_DOWN,

    // additional state for damping?

    // A note is currently playing
    // undamped
    DAMPER_UP,
  };

  struct new_state_t {
    key_state_e key_state;
    damper_state_e damper_state;

    new_state_t(key_state_e ks, damper_state_e ns)
        : key_state(ks), damper_state(ns) {}
  };

  kbd_key_t(sensor_t *sensor, key_calibration_t calibration,
            global_key_config_t global_key_config)
      : sensor(sensor), calibration(calibration),
        global_key_config(global_key_config) {}

  sample_buf_t<STRIKE_BUF_SIZE> strike_buf;

  key_state_e kstate = key_state_e::KEY_RESTING;
  damper_state_e dstate = damper_state_e::DAMPER_DOWN;
  sensor_t *sensor = nullptr;
  key_calibration_t calibration;
  global_key_config_t global_key_config;
  uint32_t strike_millis = 0;

  // this is absolutely inlined
  // because it needs to be in the hot idle loop
  // and get out of the way _fast_
  [[gnu::always_inline]] [[gnu::pure]] inline bool process_needed() {
    return this->sensor->buf.unackd != 0;
  }

  String fmt_note() { return format_note(this->calibration.spec.midi_note); }

  inline void process_sample() {
    if (!this->process_needed()) {
      return;
    }

    this->sensor->buf.unackd = 0;
    sample_t latest = this->sensor->buf.latest();
    float hammer_position = this->calibration.map(latest.height);
    sample_t corrected(hammer_position, latest.time);

    switch (this->kstate) {
    case key_state_e::KEY_RESTING:
      if (hammer_position > this->global_key_config.active) {
        this->kstate = key_state_e::KEY_READY;
        this->sensor->priority = sensor_t::poll_priority_e::CRITICAL;
      } else {
        // remain in state
      }
      break;
    case key_state_e::KEY_READY:
      if (hammer_position > this->global_key_config.letoff) {
        // go to critical
#ifdef YAHP_DEBUG
        Serial.printf("Goes to critical with value %f\r\n", hammer_position);
#endif
        this->strike_buf.add_sample(corrected); // don't lose that one!
        this->strike_buf.clear();               // ready it here
        this->kstate = key_state_e::KEY_CRITICAL;
      } else if (hammer_position < this->global_key_config.active) {
        // drop to resting
        //Serial.printf("sensor %d goes to resting \r\n",
                      //this->sensor->sensor_id);
        this->sensor->priority = sensor_t::poll_priority_e::RELAXED;
        this->kstate = key_state_e::KEY_RESTING;
      }
      break;
    case key_state_e::KEY_CRITICAL:
      this->strike_buf.add_sample(corrected);

      if (hammer_position > this->global_key_config.strike) {
        Serial.printf(
            "Processes strike after position reaches %f, strike is %f\r\n",
            hammer_position, this->global_key_config.strike);
        // go to strike
        this->kstate = key_state_e::KEY_STRIKING;
        this->strike_millis = millis();
        this->process_strike();
        Serial.println("done with strike");
      } else if (hammer_position < this->global_key_config.repetition) {
        this->kstate = key_state_e::KEY_READY;
      }

      break;
    case key_state_e::KEY_STRIKING:
      bool settled = (millis() - this->strike_millis) > SETTLE_DELAY_MS;
      if (hammer_position < this->global_key_config.repetition && settled) {
        Serial.println("Done with strike, and settled, so goes to ready");
        // only one way out \:)
        this->kstate = key_state_e::KEY_READY;
      }

      break;
    }

    bool settled = (millis() - this->strike_millis) > SETTLE_DELAY_MS;
    switch (this->dstate) {
    case damper_state_e::DAMPER_UP:
      if (hammer_position < this->global_key_config.damper_down && settled) {
        this->dstate = damper_state_e::DAMPER_DOWN;
        this->lower_damper();
      }
      break;
    case damper_state_e::DAMPER_DOWN:
      if (hammer_position > this->global_key_config.damper_up) {
        this->dstate = damper_state_e::DAMPER_UP;
        this->lift_damper();
      }
      break;
    }
  }

  void lift_damper() {
    this->dstate = damper_state_e::DAMPER_UP;
    return; // for now
#if defined(ENABLE_MIDI)
    // does this work?
    // usbMIDI.sendAfterTouchPoly(70 + this->key_number, 1, 0);
    auto note = this->calibration.spec.midi_note;
    auto channel = this->calibration.spec.midi_channel;
    usbMIDI.sendAfterTouchPoly(note + this->global_key_config.transpose, 127,
                               channel);
#endif
  }

  void lower_damper() {

    this->dstate = damper_state_e::DAMPER_DOWN;
    Serial.printf("Damp note %s\r\n", this->fmt_note().c_str());
    Serial.printf("The current level is %d\r\n" +
                  (int32_t)this->sensor->buf.latest().height);

#if defined(ENABLE_MIDI)
    auto note = this->calibration.spec.midi_note;
    auto channel = this->calibration.spec.midi_channel;
    usbMIDI.sendNoteOff(note + this->global_key_config.transpose, 127, channel);
    // usbMIDI.send_now();
#endif
  }

  float linear_regression(vector_t<sample_t, STRIKE_BUF_SIZE> &points) {
#ifdef YAHP_DEBUG
    Serial.printf("Have %d datapoints to work with\r\n", points.size());
#endif
    // subtract time of the first point, and value of the minimum
    uint32_t min_x = points.back().time;

    /*float min_val = 1000;
    for (auto point : points) {
      if (point.value < min_val) {
        min_val = point.value;
      }
    }*/

    float sum_x = 0;
    float sum_y = 0;
    float sum_xmy = 0;
    float sum_xmx = 0;
    float n = points.size();

#ifdef YAHP_DEBUG
    Serial.println("Profile:");
#endif
    for (auto point : points) {
      float x = point.time - min_x;
      float y = point.height;
#ifdef YAHP_DEBUG
      Serial.printf("%f @ %f\r\n", y, x);
#endif
      sum_x += x;
      sum_y += y;
      sum_xmy += x * y;
      sum_xmx += x * x;
    }

    float m_x = sum_x / n;
    float m_y = sum_y / n;

    float SS_xy = sum_xmy - (n * m_y * m_x);
    float SS_xx = sum_xmx - (n * m_x * m_x);

    float b_1 = SS_xy / SS_xx;
    // float b_0 = m_y - b_1 * m_x;

    return b_1 * 1000;
  }

  void process_strike() {
    vector_t<sample_t, STRIKE_BUF_SIZE> ar;

    // do this dumbly for now, we can do an iterator or something later
    // if this costs us
    for (uint32_t i = 0; i < this->strike_buf.size; i++) {
      ar.push_back(this->strike_buf.read_nth_oldest(i));
    }

    if (ar.size() < 2) {
      Serial.println("Strike array was too small to get get a reasonable "
                     "value, is teensy overwhelmed?");
      return;
    }
    Serial.printf("Found an ar with size %d\r\n", ar.size());

    if (ar.size() < 50) {
      for (auto &val : ar) {
        Serial.printf("Corrected val: %f\r\n", val.height);
      }
    }

    float slope = this->linear_regression(ar);

    float normalized_velocity = this->map_velocity(slope);

    if (normalized_velocity > 2) {
      Serial.println("Got a very weird value processing strike, so refusing to "
                     "send loud note");
      return;
    }

    uint32_t midi_velocity = normalized_velocity * 128;
    if (midi_velocity > 127) {
      midi_velocity = 127;
    }
    if (midi_velocity == 0) {
      midi_velocity = 1;
    }

#if defined(ENABLE_MIDI)
    auto note = this->calibration.spec.midi_note;
    auto channel = this->calibration.spec.midi_channel;
    Serial.println("Slope was " + String(slope));
    Serial.println("normalized was " + String(normalized_velocity));
    Serial.println("Sends velocity: " + String(midi_velocity));
    Serial.println("Sent note on! Channel: " + String(channel));
    Serial.printf("Note on for %d\r\n", note);
    Serial.printf("Transpose value of %d\r\n",
                  this->global_key_config.transpose);
    usbMIDI.sendNoteOn(note + this->global_key_config.transpose, midi_velocity,
                       channel);
#endif
  }

  // returns a velocity clamped to [0, 1]
  // for use in midi
  float map_velocity(float v) {
    v = (v < 0) ? -v : v;
    auto &c = this->global_key_config;
    // first, normalize the value
    float range = c.max_velocity - c.min_velocity;

    float n = (v - c.min_velocity) / range;
    // abs func
    // n = (n < 0) ? -n : n;
    Serial.printf("Min %f, max %f, range %f, v %f, n %f\r\n", c.min_velocity,
                  c.max_velocity, range, v, n);

    // return n;

    return pow(n, 0.5);

    /*
    // float p1x = c.bezier_p1x;
    float p1y = c.bezier_p1y;
    // float p2x = c.bezier_p2x;
    float p2y = c.bezier_p2y;
    // float p3x = c.bezier_p3x;
    float p3y = c.bezier_p3y;
    // float p4x = c.bezier_p4x;
    float p4y = c.bezier_p4y;

    float y[4] = {p1y, p2y, p3y, p4y};

    // now we have a value in between 0 and 1
    // I know bezier curves are inputs along `t`,
    // and don't really map "perfectly" to a y = f(x),
    // but they feel fine so we're going for it
    float oy = pow(1 - n, 3) * y[0] + 3 * n * pow(1 - n, 2) * y[1] +
               3 * pow(n, 2) * (1 - n) * y[2] + pow(n, 3) * y[3];

    // oy should now ideally be between 0 and 1, but let's clamp it
    // in case the control points are set "weird"
    if (oy > 1.0) {
      oy = 1;
    } else if (oy < 0) {
      oy = 0;
    }

    return oy;*/
  }
};

#endif
