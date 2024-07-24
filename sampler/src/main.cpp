#include "calibrate.cpp"
#include "core_pins.h"
#include "sampler2.cpp"
#include <algorithm>

void eloop(String s) {
    while(true) {
        Serial.println("A failure mode was entered due to an unrecoverable circumstance:");
        Serial.println(s);
        delay(10000);
    }
}

keyboardspec_t* CFG = nullptr;


void setup() {
  auto spec = yahp_from_sd();

  // make one to save there instead
  if (spec == nullptr) {
    // need to make a new config
    Serial.println("no config exists, making a new one");
    auto cfgv = run_calibration();
    yahp_to_sd(cfgv);
  }

  spec = yahp_from_sd();

  if (spec == nullptr) {
      eloop("something is horribly wrong with the SD card!");
  }

  CFG = spec;

  // now, initialize all the runtime values to match CFG
}

void loop() {
  // are we in play mode? or waiting for configure?
}

sampler_t s;

void process_chunk() {}

void player_loop() { s.sample_round(); }
