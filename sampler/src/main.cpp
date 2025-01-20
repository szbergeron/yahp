#include "pins_arduino.h"
#define ENABLE_MIDI

#include "ADC.h"
#include "core_pins.h"
#include "usb_midi.h"
#include "utils.cpp"

#include "calibrate.cpp"
#include "keyboard.cpp"
#include "magnets.cpp"
#include "sampler2.cpp"
#include <algorithm>

// #define YAHP_DEBUG

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

    // Serial.println("Stepping idle");

    for (kbd_key_t &k : this->keyboard->keys) {
      // auto note = format_note(k.calibration.spec.midi_note);
      //  Serial.println("Processing samples for " + note);
      k.process_sample();

      if (!run_all) {
        if (micros() - start > window) {
          break;
        }
      }
    }

    /*
    for (pedal_t &p : this->keyboard->pedals) {
      p.process_sample();

      if (!run_all) {
        if (micros() - start > window) {
          break;
        }
      }
    }
    */

    if (run_all) {
      // usbMIDI.send_now();
    }
  }

  idler_t(keyboard_t *keyboard) : keyboard(keyboard) {}
};

/*char (*__kaboom)[sizeof(sampler_t)] = 1;
void kaboom_print( void )
{
    printf( "%d", __kaboom )
}*/

// static sampler_t SAMPLER;
// static keyboard_t KEYBOARD;
static uint8_t IDLER_BUF[sizeof(idler_t)];
static idler_t *IDLER = nullptr;
// union IDLER_U { uint8_t ignore, idler_t idler } IDLER;

static uint8_t SAMPLER_BUF[sizeof(sampler_t)];
static sampler_t *SAMPLER = nullptr;

static uint8_t KEYBOARD_BUF[sizeof(keyboard_t)];
static keyboard_t *KEYBOARD = nullptr;

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

void configure_adc() {
  for (int i = 0; i < KEYS_PER_BOARD; i++) {
    auto pin = pins[i];
    pinMode(pin, INPUT_PULLDOWN);
  }

  analogReadAveraging(1);
  analogReadResolution(10);

  for (int i = 0; i < 8; i++) {
    uint8_t pin = i + 5;
    Serial.printf("Configures pin %d as output for boardsetting\r\n", pin);
    pinMode(pin, OUTPUT);
  }
}

void setup() {
  for (int i = 0; i < 5; i++) {
    delay(700);
    digitalToggle(LED_BUILTIN);
    Serial.println("Waiting...");
  }
  Serial.printf("%c\r\n", 0x07);

  configure_adc();

  Serial.println("Begin setup");

  pinMode(LED_BUILTIN, OUTPUT);

  if (!SD.begin(BUILTIN_SDCARD)) {
    Serial.println("Couldn't init SD");
    eloop("failed to setup sdcard");
  }

  Serial.println("Setting up...");
  fullspec_t spec = get_spec();

  if (newline_waiting()) {
    bool test_mode = confirm("try test mode?", false);
    if (test_mode) {
      testmode_entry();
    }

    bool do_more_calibration = confirm("recalibrate some keys?", false);
    if(do_more_calibration) {
      more_calibration(spec);
      doReboot();
    }
  }

  Serial.println("Making adc/sampler/keyboard...");
  ADC adc;

  SAMPLER = new (SAMPLER_BUF) sampler_t(adc, spec.keyboard.sampler);

  Serial.println("Made sampler");

  KEYBOARD = new (KEYBOARD_BUF) keyboard_t(spec, SAMPLER);

  IDLER = new (IDLER_BUF) idler_t(KEYBOARD);

  Serial.println("Made keyboard");

  Serial.println("Setup complete, entering loop...");

  pinMode(LED_BUILTIN, OUTPUT);

  configure_adc();

  // now, initialize all the runtime values to match CFG
  // Serial.begin(0);
  // usbMIDI.begin();
}

void poll_rarely() {
  if (newline_waiting()) {
    // enter config menu?
  }
}

uint32_t lm = 0;
bool state = false;

uint32_t lc = 0;

void loop() {
  lc++;

  if (lc % 10000 == 0) {
    poll_rarely();
  }
  // discard shit
  /*while (usbMIDI.read()) {
    // ignore incoming messages
  }*/

  auto now = millis();

  if (now - lm > 400) {
#ifdef YAHP_DEBUG
    Serial.println("looping...");
#endif
    lm = now;
    state = !state;
    digitalWrite(LED_BUILTIN, state);
  }

  // Serial.println("Reached loop");
  SAMPLER->sample_round();
  // Serial.println("Finished round");
  IDLER->step_idle(0, true);
  // Serial.println("Ended loop");
}
