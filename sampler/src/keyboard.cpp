#include "ADC.h"
#include "key.cpp"
#include "pedal.cpp"
#include "utils.cpp"

#ifndef YAHP_KEYBOARD
#define YAHP_KEYBOARD

struct keyboard_t {
  vector_t<kbd_key_t, KEY_COUNT_MAX> keys;
  vector_t<pedal_t, PEDAL_COUNT_MAX> pedals;
  // why a ptr?
  // we're self referential: keys point to sensors
  // we need the location of sensors to be
  // stable, so we throw the sampler on the heap
  // so it doesn't move until destruction
  sampler_t *sampler = nullptr;
  global_key_config_t gbl;

  keyboard_t(fullspec_t spec, sampler_t *sampler)
      : sampler(sampler), gbl(spec.global) {

    // I am not going to put in the effort to make this fast,
    // it runs like once or twice and only during setup

    Serial.println("Reached primary keyboard constructor");

    for (key_spec_t &ks : spec.keyboard.keys) {
      sensor_t *sensor = this->sampler->find_sensor(ks.sensor_id);

      if (sensor == nullptr) {
        eloop(String("no sensor by this ID was found: ") + ks.sensor_id);
      }

      auto sensorspec = spec.keyboard.sampler.find_sensor(ks.sensor_id).unwrap();

      key_calibration_t calibration(ks, *sensorspec);

      kbd_key_t key(sensor, calibration, gbl);

      this->keys.push_back(key);
    }

    Serial.println("Made keys");
  }
};

#endif
