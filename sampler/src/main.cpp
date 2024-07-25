#include "ADC.h"
#include "pins_arduino.h"
#include "usb_midi.h"
#include "usb_serial.h"
#include <new>
#define YAHP_DEBUG

#include "calibrate.cpp"
#include "core_pins.h"
#include "keyboard.cpp"
#include "sampler2.cpp"
#include <algorithm>

/*struct stats_t {
  float avg_ns = 0;
  uint64_t count;

  float worst_ns = 0;

  void add_stat(uint32_t ns) {}

  bool should_run(int32_t remain_budget_ns) {
    if (remain_budget_ns > 0) {
      return true;
    }
  }

  stats_t() {}
};*/


struct idler_t {
  /*stats_t key_stats;
  stats_t pedal_stats;
  stats_t midi_stats;*/
  keyboard_t *keyboard = nullptr;

  __attribute__((always_inline)) inline void step_idle(uint32_t window_ns,
                                                       bool run_all) {
    // I know, bad, but I don't see a nanos() at the moment
    // and I'll do the translation from cycles later
    auto window = window_ns / 1000;
    auto start = micros();
    if (run_all) [[unlikely]] {
      // ignore budget \:)
    }

    for (kbd_key_t &k : this->keyboard->keys) {
      if (k.process_needed()) {
        k.process_samples();
      }

      if (micros() - start > window && !run_all) {
        break;
      }
    }

    if (run_all) [[unlikely]] {
      usbMIDI.send_now();
    }
  }

  idler_t(keyboard_t* keyboard): keyboard(keyboard) {}
};

/*char (*__kaboom)[sizeof(sampler_t)] = 1;
void kaboom_print( void )
{
    printf( "%d", __kaboom )
}*/


//static sampler_t SAMPLER;
//static keyboard_t KEYBOARD;
static uint8_t IDLER_BUF[sizeof(idler_t)];
static idler_t* IDLER = nullptr;

static uint8_t SAMPLER_BUF[sizeof(sampler_t)];
static sampler_t* SAMPLER = nullptr;

static uint8_t KEYBOARD_BUF[sizeof(keyboard_t)];
static keyboard_t* KEYBOARD = nullptr;

//static idler_t IDLER;

// call this whenever there is a "pause" in sampling, or when waiting on a
// conversion, and pass the expected amount of time to idle for
//
// note, this is not a "hard" deadline: this may overrun the window.
// do not call this expecting it to return _immediately_ once the window is
// past!
//
// if run_all is set, a _full_ round will be run regardless
// of budget
//
// this is always inlined, because it is referenced in _exactly one place_,
// and I specifically want it optimized into that loop
__attribute__((always_inline)) static inline void step_idle(uint32_t window_ns,
                                                            bool run_all) {
  IDLER->step_idle(window_ns, run_all);
}

// static ADC adc();

void setup() {
  Serial.println("Begin setup");

  Serial.setTimeout(100000);
  pinMode(LED_BUILTIN, OUTPUT);
  for (int i = 0; i < 10; i++) {
    delay(700);
    digitalToggle(LED_BUILTIN);
    Serial.println("Waiting...");
  }

  // delay(5000);
  Serial.println("Setting up...");
  auto spec = yahp_from_sd();

  Serial.println("Tried load from sd");

  // make one to save there instead
  if (spec == nullptr) {
    // need to make a new config
    Serial.println("no config exists, making a new one");
    //auto cfgv = run_calibration();
    //yahp_to_sd(cfgv);
  }

  spec = yahp_from_sd();

  Serial.println("Loaded a final");

  if (spec == nullptr) {
    eloop("something is horribly wrong with the SD card!");
  }

  Serial.println("Making adc/sampler/keyboard...");
  ADC adc;

  // SAMPLER = sampler_t(adc, spec->sampler);
  // SAMPLER{adc, spec->sampler};
  SAMPLER = new (SAMPLER_BUF) sampler_t(adc, spec->sampler);

  Serial.println("Made sampler");

  // KEYBOARD = new keyboard_t(*spec, SAMPLER);
  KEYBOARD = new (KEYBOARD_BUF) keyboard_t(spec, SAMPLER, spec->gbl);

  IDLER = new (IDLER_BUF) idler_t(KEYBOARD);

  Serial.println("Made keyboard");

  Serial.println("Setup complete, entering loop...");

  // now, initialize all the runtime values to match CFG
}

void loop() {
  // Serial.println("Reached loop");
  SAMPLER->sample_round();
  // Serial.println("Finished round");
  IDLER->step_idle(0, true);
  // Serial.println("Ended loop");
}
