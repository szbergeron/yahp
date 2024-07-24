#include "sampler2.cpp"
#include "utils.cpp"

#include "Array.h"
#include "usb_midi.h"
#include "usb_serial.h"

#ifndef YAHP_KEY
#define YAHP_KEY

uint16_t DEFAULT_BOTTOM = 270;
uint16_t DEFAULT_UP = 460;
uint16_t DEFAULT_GAP = DEFAULT_UP - DEFAULT_BOTTOM;

struct key_calibration_t {
  // all of these are normalized to
  // ideally fall within a 0-1 range,
  // though values outside of that
  // range are also completely allowed
  float active;
  float letoff;
  float strike;
  float repetition;
  float damper_up;
  float damper_down;

  key_spec_t spec;

  inline float normalize_sample(uint16_t sample) {
    float as_f = sample;
    float in_range = this->spec.max_val - this->spec.min_val;

    as_f -= this->spec.min_val;
    float normalized = as_f / in_range;

    return normalized;
  }

  inline void verify_states() {
    if (this->active > this->damper_down) {
      Serial.printf("active %f is greater than dd %f\n", this->active,
                    this->damper_down);
    }

    if (this->damper_down > this->damper_up) {
      Serial.printf("dd %f is greater than du %f\n", this->active,
                    this->damper_down);
    }

    if (this->damper_up > this->strike) {
      Serial.printf("du %f is greater than strike %f\n", this->active,
                    this->damper_down);
    }

    if (this->active > this->repetition) {
      Serial.printf("active %f is greater than repetition %f\n", this->active,
                    this->damper_down);
    }

    if (this->repetition > this->letoff) {
      Serial.printf("repetition %f is greater than letoff %f\n", this->active,
                    this->damper_down);
    }

    if (this->letoff > this->strike) {
      Serial.printf("letoff %f is greater than strike %f\n", this->active,
                    this->damper_down);
    }
  }

  key_calibration_t(key_spec_t &spec, global_key_config_t &gbl)
      : spec(spec), active(gbl.active), letoff(gbl.letoff), strike(gbl.strike),
        repetition(gbl.repetition), damper_up(gbl.damper_up),
        damper_down(gbl.damper_down) {}
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

  key_state_e kstate = key_state_e::KEY_RESTING;
  damper_state_e dstate = damper_state_e::DAMPER_DOWN;
  sensor_t *sensor = nullptr;
  key_calibration_t calibration;
  global_key_config_t *global_key_config;

  sample_buf_t<1024> strike_buf;

  inline new_state_t new_state_given(sample_t sample) {
    auto &c = this->calibration;

    auto normalized = c.normalize_sample(sample.value);

    if (normalized <= c.active) {
      return new_state_t(key_state_e::KEY_RESTING, damper_state_e::DAMPER_DOWN);
    }

    damper_state_e ds;
    key_state_e ks;

    if (normalized > c.damper_up) {
      ds = damper_state_e::DAMPER_UP;
    } else if (normalized < c.damper_down) {
      ds = damper_state_e::DAMPER_DOWN;
    } else {
      ds = this->dstate;
    }

    if (normalized > c.active && normalized <= c.repetition) {
      ks = key_state_e::KEY_READY;
    } else if (normalized > c.repetition && normalized <= c.letoff) {
      // keep it as it is, either coming up (READY),
      // or recovering from a strike (STRIKING)
      ks = this->kstate;
    } else if (normalized > c.letoff && normalized <= c.strike) {
      ks = key_state_e::KEY_CRITICAL;
    } else if (normalized > c.strike) {
      ks = key_state_e::KEY_STRIKING;
    }

    return new_state_t(ks, ds);
  }

  void process_samples() {
    if (this->sensor->buf.unackd == 0) {
      return; // no new info
    }

    auto latest = this->sensor->buf.latest();

    auto ns = this->new_state_given(latest);

    if (ns.key_state == key_state_e::KEY_RESTING) {
      this->sensor->priority = sensor_t::poll_priority_e::RELAXED;
    } else if (ns.key_state == key_state_e::KEY_READY &&
               this->kstate == key_state_e::KEY_RESTING) {
      this->sensor->priority = sensor_t::poll_priority_e::CRITICAL;
    } else if (ns.key_state == key_state_e::KEY_CRITICAL) {
      // start collecting samples
      for (int32_t i = (int32_t)this->sensor->buf.unackd - 1; i >= 0; i--) {
        this->strike_buf.add_sample(this->sensor->buf.read_nth_oldest(i));
        this->sensor->buf.unackd = 0;
      }
    } else if (ns.key_state == key_state_e::KEY_STRIKING &&
               this->kstate == key_state_e::KEY_CRITICAL) {
      // note played, grab samples and process velocity
      this->process_strike();
      this->strike_buf.clear();
    } else if (ns.key_state == key_state_e::KEY_READY &&
               this->kstate == key_state_e::KEY_CRITICAL) {
      // clear sample buf, go back to "waiting"
      this->strike_buf.clear();
    }

    this->kstate = ns.key_state;

    if (ns.damper_state == damper_state_e::DAMPER_UP &&
        this->dstate == damper_state_e::DAMPER_DOWN) {
      this->lift_damper();
    } else if (ns.damper_state == damper_state_e::DAMPER_DOWN &&
               this->dstate == damper_state_e::DAMPER_UP) {
      this->lower_damper();
    }

    this->dstate = ns.damper_state;
  }

  void lift_damper() {
#if defined(ENABLE_MIDI)
    // does this work?
    usbMIDI.sendAfterTouchPoly(70 + this->key_number, 1, 0);
#endif
  }

  void lower_damper() {
    // Serial.println("Damp note " + this->format_note());
#if defined(ENABLE_MIDI)
    usbMIDI.sendNoteOff(70 + this->key_number, 127, 0);
    // usbMIDI.send_now();
#endif
  }

  float linear_regression(Array<sample_t, 1024> &points) {
    // subtract time of the first point, and value of the minimum
    float min_x = points.back().time;

    float min_val = 1000;
    for (auto point : points) {
      if (point.value < min_val) {
        min_val = point.value;
      }
    }

    float sum_x = 0;
    float sum_y = 0;
    float sum_xmy = 0;
    float sum_xmx = 0;
    float n = points.size();
    for (auto point : points) {
      float x = (point.time - min_x);
      float y = (point.value - min_val);
      sum_x += x;
      sum_y += y;
      sum_xmy += x * y;
      sum_xmx += x * x;
    }

    float m_x = sum_x / n;
    float m_y = sum_y / n;

    // Serial.println("Means: " + String(m_x) + ", " + String(m_y));

    float SS_xy = sum_xmy - (n * m_y * m_x);
    float SS_xx = sum_xmx - (n * m_x * m_x);

    float b_1 = SS_xy / SS_xx;
    // float b_0 = m_y - b_1 * m_x;

    return b_1;
  }

  void process_strike() {
    Array<sample_t, 1024> ar;

    // do this dumbly for now, we can do an iterator or something later
    // if this costs us
    for (uint32_t i = 0; i < this->strike_buf.size; i++) {
      ar.push_back(this->strike_buf.read_nth_oldest(i));
    }

    float slope = this->linear_regression(ar);
    float normalized_velocity = this->map_velocity(slope);

    uint32_t midi_velocity = normalized_velocity * 128;
    if (midi_velocity > 127) {
      midi_velocity = 127;
    }

#if defined(ENABLE_MIDI)
    usbMIDI.sendNoteOn(this->calibration->spec.midi_note, midi_velocity,
                       this->calibration->spec.midi_channel);
#endif
  }

  // returns a velocity clamped to [0, 1]
  // for use in midi
  float map_velocity(float v) {
    auto &c = *this->global_key_config;
    // first, normalize the value
    float range = c.max_velocity - c.min_velocity;

    float n = (v - c.min_velocity) / range;
    n = (n < 0) ? -n : n;

    float p1x = c.bezier_p1x;
    float p1y = c.bezier_p1y;
    float p2x = c.bezier_p2x;
    float p2y = c.bezier_p2y;
    float p3x = c.bezier_p3x;
    float p3y = c.bezier_p3y;
    float p4x = c.bezier_p4x;
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

    return oy;
  }
};

#endif
