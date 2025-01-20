#include "utils.cpp"
#include "vector.cpp"
#include "../../sampler_generated.h"

#ifndef YAHP_TYPES
#define YAHP_TYPES

/*struct sample_t {

  float height;
  uint32_t time;

  sample_t(float height, uint32_t time) : height(height), time(time) {}

  sample_t() : height(0), time(0) {}
};*/

typedef YAHP::TsySample sample_t;

#endif
