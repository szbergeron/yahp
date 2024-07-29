#include "utils.cpp"


#include "core_pins.h"
#include "elapsedMillis.h"
#include <cstddef>
#include "vector.cpp"

#ifndef YAHP_MAGNETS
#define YAHP_MAGNETS

const size_t MAGNET_INTERPOLATOR_POINTS = 16;

struct longsample_t {
  float value;
  uint32_t time;

  longsample_t(float value, uint32_t time) : value(value), time(time) {}
  longsample_t() : value(0), time() {}
};

struct point_t {
  float x;
  float y;

  point_t(JsonObject j) : x(j["x"]), y(j["y"]) {}

  JsonDocument to_json() { JsonDocument j; }

  point_t(float x, float y) : x(x), y(y) {}
};

template <uint32_t POINTS> struct interpolater_t {
  // point_t points[POINTS];
  vector_t<point_t, POINTS> points;

  inline float interpolate(float x) {
    int idx = 0;
    for (; idx < POINTS; idx++) {
      if (this->points[idx].x > x) [[unlikely]] {
        break;
      }
    }

    // point_t p_a;
    // point_t p_b;

    uint32_t i_a = 0;
    uint32_t i_b = 0;

    // branchless because it's fun
    idx -= 1 * (idx == POINTS);
    idx += 1 * (idx == 0);

    /*if (idx == POINTS) [[unlikely]] {
      i_a = idx - 2;
      i_b = idx - 1;
    } else if (idx == 0) [[unlikely]] {
      i_a = idx;
      i_b = idx + 1;
    } else {
      // linear between the points
      i_a = idx - 1;
      i_b = idx;
    }*/

    auto p_a = this->points[i_a];
    auto p_b = this->points[i_b];

    auto top = (p_a.y * (p_b.x - x)) + (p_b.y * (x - p_a.x));
    auto bottom = p_b.x - p_a.x;

    return top / bottom;
  }

  template <uint32_t INPUT_POINTS>
  interpolater_t(vector_t<point_t, INPUT_POINTS> inputs) {
    vector_t<point_t, INPUT_POINTS> sorted;

    // first, sort it by x
    for (int i = 0; i < inputs.size(); i++) {
      for (int j = i + 1; i < inputs.size(); j++) {
        auto p_a = inputs[i];
        auto p_b = inputs[j];

        if (p_a.x > p_b.x) {
          inputs[i] = p_b;
          inputs[j] = p_a;
        }
      }
    }

    // now, split into ranges
    size_t stride = inputs.size() / POINTS;

    for (size_t range_start; range_start < inputs.size();
         range_start += stride) {
      size_t count = 0;
      float sum_x = 0;
      float sum_y = 0;
      for (size_t i = range_start;
           i < inputs.size() && i < range_start + stride; i++) {
        count++;
        auto p = inputs[i];
        sum_x += p.x;
        sum_y += p.y;
      }

      float x = sum_x / count;
      float y = sum_y / count;

      this->points.push_back({x, y});
    }
  }

    // A very basic "default constructed" interpolater
    // that pretty much barely works
  interpolater_t(float min, float max) {
      this->points.push_back(point_t(min, 0));
      this->points.push_back(point_t(max, 1));
  }

  interpolater_t(JsonArray j) {
    auto ja = j;

    for (auto ent : ja) {
      this->points.push_back(point_t(ent.as<JsonObject>()));
    }
  }

  JsonDocument to_json() {
    JsonDocument d;

    d["sensor_id"] = this->sensor_id;
    d["max_val"] = this->max_val;
    d["min_val"] = this->min_val;
    d["midi_note"] = this->midi_note;
    d["midi_channel"] = this->midi_channel;

    return d;
  }
};

/*line_t linear_regression(Array<point_t, 10> &points) {
  Serial.printf("Have %d datapoints to work with\r\n", points.size());
  // subtract time of the first point, and value of the minimum
  uint32_t min_x = points.back().time;

  /float min_val = 1000;
  for (auto point : points) {
    if (point.value < min_val) {
      min_val = point.value;
    }
  }*

  float sum_x = 0;
  float sum_y = 0;
  float sum_xmy = 0;
  float sum_xmx = 0;
  float n = points.size();
  Serial.println("Profile:");
  for (auto point : points) {
    float x = (point.time - min_x);
    // float y = (point.value - min_val);
    // float y = this->calibration.normalize_sample(point.value);
    float y = point.value;
    // print_normalized(y);
    Serial.printf("%f @ %f\r\n", y, x);
    // Serial.flush();
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
   float b_0 = m_y - b_1 * m_x;

  return //;
}*/

/*template <uint32_t PIECES>
struct piecewise_t {
    line_t pieces[PIECES];
    float min;
    float max;
};

struct magnetic_compensator {
  float sensor_min; // amount to initially offset by
  float sensor_absolute_max;
};*/

template <uint32_t BUFSIZE> struct longsample_buf_t {
  longsample_t buffer[BUFSIZE];

  // begin points at the newest sample,
  // and (begin + 1) % SAMPLE_BUFFER_LENGTH is the next newest
  uint32_t begin = 0;
  uint32_t size = 0;

  uint32_t unackd;

  void add_sample(longsample_t sample) {
    /*this->begin =
        (this->begin + SAMPLE_BUFFER_LENGTH - 1) % SAMPLE_BUFFER_LENGTH;*/
    this->begin++;
    this->begin %= BUFSIZE;

    this->unackd += 1;

    this->buffer[this->begin] = sample;
    if (this->size < BUFSIZE) {
      this->size++;
    }
  }

  void clear() {
    this->begin = 0;
    this->size = 0;
    this->unackd = 0;
  }

  // if n is greater than buffer length,
  // then the oldest still held sample is returned
  // if no samples exist, the sample will be "zero"
  longsample_t read_nth_oldest(uint32_t n) {
    // Serial.println("N is: " + String(n));
    //  clamp n
    if (n > BUFSIZE) [[unlikely]] {
      n = BUFSIZE;
    }

    auto res = longsample_t{0, 0};
    if (this->size == 0) {
      // keep default
    } else {
      auto start = this->begin;
      auto with_room = start + BUFSIZE;
      auto offsetted = with_room - n;
      auto idx = offsetted % BUFSIZE;
      // Serial.println("Returns idx: " + String(idx));
      res = this->buffer[idx];
      // res = this->buffer[(n + this->begin) % SAMPLE_BUFFER_LENGTH];
    }

    // print_value(res.value);

    return res;
  }
};

float linear_regression(vector_t<longsample_t, 10> &points) {
  Serial.printf("Have %d datapoints to work with\r\n", points.size());
  // subtract time of the first point, and value of the minimum
  uint32_t min_x = points.back().time;

  /*float min_val = 1000;
  for (auto point : points) {
    if (point.value < min_val) {
      min_val = point.value;
    }
  }*/

  float sum_x = 0;
  float sum_y = 0;
  float sum_xmy = 0;
  float sum_xmx = 0;
  float n = points.size();
  Serial.println("Profile:");
  for (auto point : points) {
    float x = (point.time - min_x);
    // float y = (point.value - min_val);
    // float y = this->calibration.normalize_sample(point.value);
    float y = point.value;
    // print_normalized(y);
    Serial.printf("%f @ %f\r\n", y, x);
    // Serial.flush();
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

  return b_1 * 1000;
}

struct position_t {
  float distance;
  float flux;
  float time;
};

/*bool is_bound_point(longsample_buf_t<4096> &buf, uint32_t pos) {
    // start searching backwards, for start
    Array<longsample_t, 10> before;
    Array<longsample_t, 10> after;

    for(int j = pos; j < buf.size && !before.full(); j++) {
        before.push_back(buf.read_nth_oldest(j));
    }

    for(int j = pos; j > 0 && !after.full(); j--) {
        after.push_back(buf.read_nth_oldest(j));
    }

    //float lrb = linear_regression(before);
    //float lra = linear_regression(before);
}*/

static interpolater_t<MAGNET_INTERPOLATOR_POINTS> drop_test(uint8_t bnum,
                                                            uint8_t pin) {

  Serial.println("This step calibrates the native hammer range,");
  Serial.println("as well as the voltage response curve with respect to "
                 "distance of the sensors");

  set_board(bnum);
  delayMicroseconds(10);
  analogReadAveraging(16);
  analogReadResolution(10);
  pinMode(pin, INPUT);

  // first collect the down position,
  // to serve as a reference point for middle-crossing
  float initial_resting = analogRead(pin);
  float rough_max = initial_resting;

  Serial.println("Now lift the hammer, until you can feel it pressing on the "
                 "felt but without too much effort");
  Serial.println("Don't try to deform the felt--when the hammer drops, it "
                 "should not be accelerated by the felt");
  Serial.println("You can release the hammer once you've done so, and then "
                 "press the enter key");
  Serial.clear();

  while (true) {
    float v = analogRead(pin);
    rough_max = max(rough_max, v);

    if (newline_waiting()) {
      break;
    }
  }

  Serial.printf("Found rough min %f and rough max %f\r\n", initial_resting,
                rough_max);
  if (!confirm("Is this acceptable?", true)) {
    Serial.println("Retrying...");
    return drop_test(bnum, pin);
  }

  Serial.println("Now, we need to generate a fall profile for the hammer.");
  Serial.println("This will continuously sample sensor voltage, and will "
                 "trigger a capture once");
  Serial.println("the voltage falls to a 25% mark, which should be roughly "
                 "at half-travel.");
  Serial.println();
  Serial.println("First, secure the back of the key itself. If the key is "
                 "allowed to move with");
  Serial.println("the hammer, it affects the fall speed such that it no "
                 "longer sufficiently");
  Serial.println("obeys free-body, free-fall properties. Make sure the jack "
                 "and whippen");
  Serial.println("do not follow the hammer up when you lift it, and do not "
                 "make contact");
  Serial.println("again until the hammer falls back to its resting position");
  Serial.println();
  Serial.println("Whenever you're ready, lift the hammer back to about the "
                 "same point as before.");

  // just make it nice and big
  longsample_buf_t<4096> buf;

  // first, start sampling into a ring
  longsample_buf_t<512> ring;

  // need to detect the first falling edge when dropping the hammer,
  // but don't want to accidentally trigger on noise,
  // so set a drop threshold

  while (true) {
    float v = analogRead(pin);
    if (v >= rough_max) {
      break;
    }
  }

  Serial.println("Now, release the hammer and allow it to fall until it rests");
  auto start = micros();

  bool passed_midpoint = false;
  float midpoint = ((rough_max - initial_resting) / 2) + initial_resting;
  // until buf full
  uint32_t midpoint_pos = 0;
  while (buf.size < 4095) {
    float v = analogRead(pin);
    uint32_t t = micros() - start;
    longsample_t s(v, t);
    if (!passed_midpoint) {
      ring.add_sample(s);

      if (v < midpoint) {
        passed_midpoint = true;
        // move them all over, do processing later
        for (int i = 0; i < ring.size; i++) {
          buf.add_sample(ring.read_nth_oldest(i));
        }
      }
    } else {
      midpoint_pos++;
      buf.add_sample(s);
    }
  }

  Serial.println("Data gathered, processing...");

  // probably won't take this many samples, but can still do

  uint32_t start_pos;
  uint32_t end_pos;

  // need to find the points that mark the start and end
  for (int i = midpoint_pos; i < buf.size - 10; i++) {
    if (buf.read_nth_oldest(i).value <= initial_resting + 10) {
      start_pos = i;
      break;
    }
  }

  for (int i = midpoint_pos; i > 10; i--) {
    auto val = buf.read_nth_oldest(i).value;
    if (val > rough_max - 10) {
      end_pos = i;
      break;
    }
  }

  vector_t<longsample_t, 4096> fall;

  // now, we have our bounds
  for (int i = start_pos; i >= end_pos; i--) {
    // backwards, because buf is a forgetful queue
    fall.push_back(buf.read_nth_oldest(i));
  }

  // now, figure out what the points _should_ be
  // in terms of actual "distance" according to
  // laws of gravity

  float g = -9.81;
  float initial_velocity = 0;
  auto t_0 = fall.at(0).time;

  vector_t<position_t, 4096> datapoints;

  // TODO: is this right? do I need to know if these are 0 vs uninit?
  float distance_max = 0;

  for (auto &sample : fall) {
    uint32_t t_i = sample.time - t_0;
    float t_if = (float)t_i / 1000000; // convert micros to seconds

    float height = 0.5 * g * (t_if * t_if) + (initial_velocity * t_if);

    position_t p;
    p.distance =
        -height; // position goes down, but distance goes up as hammer falls
    p.flux = sample.value;

    // only falling downward, I hope
    distance_max = min(p.distance, distance_max);
  }

  // normalize distance to between 0 and 1,
  // 1 being the highest (closest to sensor), 0 being the "bottom"
  // the datapoints currently should have a max near 0,
  // and a min somewhere in the negatives
  for (auto &p : datapoints) {
    float cur_p = p.distance;
    p.distance = cur_p / distance_max;
  }

  // now we have a bunch of datapoints mapping flux density
  // to distance, regardless of time
  //
  // flux density follows the inverse-cube law
  //
}

#endif
