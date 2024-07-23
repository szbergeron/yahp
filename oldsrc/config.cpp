#include "ArduinoJson/Array/JsonArray.hpp"
#include "ArduinoJson/Document/JsonDocument.hpp"
#include "ArduinoJson/Numbers/JsonFloat.hpp"
#include "ArduinoJson/Object/JsonObject.hpp"
#include "ArduinoJson/Variant/VariantTo.hpp"
#include "keylayouts.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <ArduinoJson.hpp>
#include <Array.h>
#include <SD.h>
#include <cstddef>

typedef uint8_t key_id_t;

const uint32_t KEY_COUNT = 128;

struct interference_contrib_t {
  key_id_t key_id;
  float max_contrib;

  // technically,
  // B(m, r, λ) = ((μ_0 * m) / (4π r^3)) * sqrt(1 + 3sin^2(λ))
  // or some other nerd shit
  //
  // lets try to approximate
  //
  // if we can't reasonably measure r, or λ, and if B is hard to
  // measure without specialized equipment, then either we could
  // try to approximate all of them exactly and then fully evaluate
  // the equation (ew), or we could embrace that we have bad data,
  // bad brains, and bad friends, and just wing it with
  // an approximate equation to match our approximate data
  //
  // μ_0, π, and m are all (I hope?) constants.
  // At least, I hope you're using the same magnet
  // dimensions/strength for all magnets
  //
  // If we're doing dumb-person math that should mean
  // we can take an arbitrary constant on top of the fraction
  // and turn it into just
  //
  // B(r, λ) = (1 / r^3) * sqrt(1 + 3sin^2(λ))
  //
  // We also will have a really hard time measuring λ exactly,
  // but we can guesstimate that at the start of motion,
  // it should be near 0 degrees to begin with
  // (mostly aligned with the axis of the dipole).
  // When the hammer is at the top, the latitude should
  // be close to 90 degrees
  //
  // The tough part is `r`. We know at some point it
  // goes to close-to-zero, and this whole thing should theoretically
  // go to basically infinity. But also magnets take up space
  // So, r doesn't actually go to zero
  //
  // Also, sqrt(1 + 3sin^2(λ)) from 0 to 1 is pretty darn close to a
  // plain sine wave once we shift/scale a little bit, letting us
  // avoid some expensive inner operations. We also never
  // observe a value that is outside of (0, 90), and
  // while we could go nuts with perfectly calculating this,
  // we still don't have an `r` nailed down, nor an exact latitude.
  //
  // I'm gonna use a linear approximation, and if you don't like it
  // then feel free to try to fit all that theoretical shit
  // into a budget of 50 some-odd clock cycles on a 32 bit arm core
  // that's only barely capable of FP math.
  //
  // If I'm feeling fancy, I'll do a bezier/hermite later to add
  // those slim tails
  //
  // self_val is the latest value for _this_ key
  float scaled_negative_contrib(float self_val, float self_val_max) {
    // assume we have a default contrib of zero,
    // and contrib scales linearly up to the max_contrib

    float v = (self_val / self_val_max);
    v = v * this->max_contrib;

    return v;
  }
};

struct key_config_t {
  uint32_t resting;
  uint32_t max; // the maximum value

  uint16_t key_id;
  uint8_t board_number; // which board this key is on
  uint8_t
      index_on_board; // which sensor (out of roughly "16") this is on the board
  uint8_t note;       // midi note

  key_config_t() {
  }

  key_config_t(JsonVariant jv)
      : resting(jv["resting"]), max(jv["max"]), key_id(jv["key_id"]), board_number(jv["board_number"]), index_on_board(jv["index_on_board"]), note(jv["note"]) {}

  JsonVariant to_json() {
      JsonDocument d;

      d["resting"] = this->resting;
      d["max"] = this->max;
      d["key_id"] = this->key_id;
      d["board_number"] = this->board_number;
      d["index_on_board"] = this->index_on_board;
      d["note"] = this->note;

      return d;
  }
};

struct keys_config_t {
  // what if you have a crazy bosendorfer or something
  Array<key_config_t, KEY_COUNT> keys;

  keys_config_t(JsonArray j) {
    for (auto jv : j) {
      key_config_t kc(jv);

      this->keys.push_back(kc);
    }
  }

  JsonVariant to_json() {
    JsonArray ja;
    for (auto &kc : this->keys) {
      ja.add(kc.to_json());
    }

    return ja;
  }
};

struct compensation_config_t {
  Array<float, 5> contrib;

  compensation_config_t(ArduinoJson::JsonArray ja) {
    Array<float, 5> c;

    for (ArduinoJson::JsonVariant v : ja) {
      auto f = v.as<ArduinoJson::JsonFloat>();

      c.push_back(f);
    }
  }

  JsonVariant to_json() {
    JsonArray a;

    for (auto &v : this->contrib) {
      a.add(v);
    }

    return a;
  }

  float recalculate(float val, Array<float, 5> vals_left,
                    Array<float, 5> vals_right) {
    // TODO
    return val;
  }
};

struct velocity_t {
    uint16_t val;

    velocity_t(uint16_t val): val(val) {}

    uint16_t midi2() {
        return this->val;
    }

    uint8_t midi() {
        return this->val >> 8;
    }
};

// responsible for doing velocity mapping 
struct curve_t {
    float floor = 0;
    float ceiling = 0;

    float exponent = 1;
    float scale;

    curve_t() {
        // we want the floor vel
        // to result in a velocity of 0,
        // and a ceiling val to result in
        // uint16_t::MAX

        //float uncorrected = pow(this->ceiling - this->floor, this->exponent);

        //return 65535.0;
    }

    velocity_t map(float raw_velocity) {
    }
};

struct curves_config_t {
    Array<curve_t, KEY_COUNT> per_key;
    curve_t global;

    curves_config_t() {
    }
};

struct keyboard_config_t {
  compensation_config_t compensation;
  keys_config_t keys;
  curves_config_t curves;
};
