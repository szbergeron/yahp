#include "ADC.h"
#include "key.cpp"

#ifndef YAHP_KEYBOARD
#define YAHP_KEYBOARD

struct keyboard_t {
  vector_t<kbd_key_t, KEY_COUNT_MAX> keys;
  // why a ptr?
  // we're self referential: keys point to sensors
  // we need the location of sensors to be
  // stable, so we throw the sampler on the heap
  // so it doesn't move until destruction
  sampler_t *sampler = nullptr;
  global_key_config_t gbl;

  keyboard_t(keyboardspec_t *spec, sampler_t *sampler, global_key_config_t gbl)
      : sampler(sampler), gbl(gbl) {
    // I am not going to put in the effort to make this fast,
    // it runs like once or twice and only during setup

    Serial.println("Reached primary keyboard constructor");

    for (key_spec_t &ks : spec->keys) {
      sensor_t *sensor = this->sampler->find_sensor(ks.sensor_id);

      if (sensor == nullptr) {
        eloop(String("no sensor by this ID was found: ") + ks.sensor_id);
      }

      key_calibration_t calibration(ks);

      kbd_key_t key(sensor, calibration, gbl);

      this->keys.push_back(key);
    }

    Serial.println("Made keys");
  }
};

#endif
