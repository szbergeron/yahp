#ifndef YAHP_KEY
#define YAHP_KEY

#include "sampler2.cpp"
#include "utils.cpp"
//#include <coroutine>

uint16_t DEFAULT_BOTTOM = 270;
uint16_t DEFAULT_UP = 460;
uint16_t DEFAULT_GAP = DEFAULT_UP - DEFAULT_BOTTOM;

struct key_calibration_t {
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

  key_calibration_t() {}
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

  enum class note_state_e {
    // No note is active,
    NOTE_OFF,

    // additional state for damping?

    // A note is currently playing
    // undamped
    NOTE_ON,
  };

  key_state_e state;
  note_state_e nstate;
  sensor_t *sensor;
  key_calibration_t calibration;

  void set_key_state(key_state_e ns) {
    switch (ns) {

    case key_state_e::KEY_RESTING:
      this->sensor->priority = sensor_t::poll_priority_e::RELAXED;
      break;
    case key_state_e::KEY_READY:
    case key_state_e::KEY_CRITICAL:
    case key_state_e::KEY_STRIKING:
      this->sensor->priority = sensor_t::poll_priority_e::CRITICAL;
      break;
    }

    this->state = ns;
  }

  // this is where we handle state transitions
  // and queue up event dispatches
  //
  void process_samples() {
  }

  void process_samples_inner() {
    auto latest = this->sensor->buf.read_nth_oldest(0);
    bool note_played = false;
    bool damper_on = false;
    bool damper_off = false;

    switch (this->state) {
    case key_state_e::KEY_RESTING:
      if (latest.value < this->calibration.ready_th_on) {
        Serial.println("KEY ACTIVATES");

        // TODO: lift damper
        this->state = key_state_e::KEY_READY;
      }
      break;
    case key_state_e::KEY_READY:
      if (latest.value > this->calibration.damper_on_th_on) {
        damper_on = true;
      } else if (latest.value < this->calibration.damper_on_th_off) {
        damper_off = true;
      }
      if (latest.value > this->calibration.ready_th_off) {
        Serial.println("Goes back to resting...");
        this->set_key_state(key_state_e::KEY_RESTING);
      } else if (latest.value < this->calibration.letoff_th_on) {
        // passed letoff, now in critical
        this->set_key_state(key_state_e::KEY_CRITICAL);
        Serial.println("letoff lets go");
      } else if (latest.value > this->calibration.ready_th_off) {
        Serial.println("key up");
        this->set_key_state(key_state_e::KEY_RESTING);
      }
      break; // no fallthrough since we want at least two samples within
             // CRITICAL before STRIKE
    case key_state_e::KEY_CRITICAL:
      if (latest.value < this->calibration.strike_th_on) {
        Serial.println("STRIKE");
        // Serial.println("critical got note!");
        // Serial.println(latest.value);
        // Serial.println(this->calibration.strike_th_on);
        //  when the magic happens
        note_played = true;
        this->set_key_state(key_state_e::KEY_STRIKING);
      } else if (latest.value > this->calibration.letoff_th_off) {
        // note abandoned, but keep it active
        Serial.println("note abandoned");
        this->set_key_state(key_state_e::KEY_READY);
      }
      break;
    case key_state_e::KEY_STRIKING:
      if (latest.value > this->calibration.letoff_th_off) {
        Serial.println("letoff resets");
        // outside of CRITICAL, so allow new note to play
        this->set_key_state(key_state_e::KEY_READY);
      }
    }

    if (note_played) {
      this->strike_velocity();
    }

    if (damper_on) {
      this->damp();
    }

    if (damper_off) {
      this->undamp();
    }

    if (damper_on && damper_off) {
      errorln("bro");
      delay(1000);
    }
  }

  void undamp() {

    if (this->nstate == note_state_e::NOTE_ON) {
      // do nothing if already undamped
      return;
    }

    this->nstate = note_state_e::NOTE_ON;

    Serial.println("Undamp...");
    // TODO: figure out how to send a damper off for an unplayed key?
    return;

#if defined(ENABLE_MIDI)
    usbMIDI.sendAfterTouchPoly(70 + this->key_number, 1, 0);
#endif
  }

  void damp() {
    if (this->nstate == note_state_e::NOTE_OFF) {
      // do nothing if already damped
      return;
    }

    this->nstate = note_state_e::NOTE_OFF;

    Serial.println("Damper back on");
#if defined(ENABLE_MIDI)
    usbMIDI.sendNoteOff(70 + this->key_number, 127, 0);
    // usbMIDI.send_now();
#endif
  }

  float linear_regression(Array<sample_t, SAMPLE_BUFFER_LENGTH> points) {
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

    return b_1 * -10000;
  }

  // calculate the velocity of a strike
  // precondition: a strike is (and should be) triggered
  // n is the sample to start from, marking the "transient"
  void strike_velocity() {
    Array<sample_t, SAMPLE_BUFFER_LENGTH> samples;

    uint32_t mins = UINT32_MAX;
    uint32_t maxs = 0;

    for (int i = 0; i < SAMPLE_BUFFER_LENGTH; i++) {
      auto sample = this->sensor->buf.read_nth_oldest(i);

      if (sample.value > this->calibration.letoff_th_on) {
        // don't include it -- either noise or too old
        // TODO: break out if multiple out of range
      } else if (sample.value < this->calibration.strike_th_on) {
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

    // Serial.println("Strike detected!");

    float velocity = linear_regression(samples);
    // we want to curve map this since out of the box it's dumb

    // float new_velocity = pow(velocity, 1) * 7 - 8;
    float new_velocity = velocity / 5;

    int first_time = 0;
    if (!samples.empty()) {
      first_time = samples.back().time;
    }

    Serial.println("Velocity: " + String(new_velocity));
    Serial.println("Strike profile:");
    Serial.printf("Threshold for KEY_CRITICAL: %d\r\n",
                  this->calibration.letoff_th_on);
    Serial.printf("Threshold for KEY_STRIKING: %d\r\n",
                  this->calibration.strike_th_on);
    Serial.println("Max observed: " + String(maxs));
    Serial.println("Min observed: " + String(mins));

#ifdef PROFILE_ENABLED
    // float points = min((float)samples.size(), 32.0);
    // float gap = max((float)samples.size() / 32.0, 1.0);
    for (int i = samples.size() - 1; i >= 0; i -= 1) {
      Serial.printf("%10d -- ", samples[i].time);
      Serial.printf("%5d -- ", samples[i].time - first_time);
      print_value(samples[i].value);
    }
#endif

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
#endif

    this->nstate = note_state_e::NOTE_ON;
  }
};

#endif
