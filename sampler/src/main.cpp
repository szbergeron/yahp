#include "sampler2.cpp"
#include "calibrate.cpp"

void setup() {
}

void loop() {
    // are we in play mode? or waiting for configure?
}

sampler_t s;

void process_chunk() {
}

void player_loop() {
    s.sample_round();
}
