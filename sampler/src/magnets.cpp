#include "utils.cpp"

#include "core_pins.h"
#include "elapsedMillis.h"
#include "vector.cpp"
#include <cstddef>

#ifndef YAHP_MAGNETS
#define YAHP_MAGNETS

const size_t MAGNET_INTERPOLATOR_POINTS = 32;

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
  point_t(JsonVariant j) : point_t(j.as<JsonObject>()) {}

  JsonDocument to_json() {
    JsonDocument j;
    j["x"] = this->x;
    j["y"] = this->y;
    return j;
  }

  point_t(float x, float y) : x(x), y(y) {}
};

template <uint32_t POINTS> struct interpolater_t {
  // point_t points[POINTS];
  vector_t<point_t, POINTS> points;

  inline float interpolate(float x) {
    uint32_t idx = 0;
    for (; idx < POINTS; idx++) {
      if (this->points[idx].x > x) {
        break;
      }
    }

    // point_t p_a;
    // point_t p_b;

    uint32_t i_a = 0;
    uint32_t i_b = 0;

    // branchless because it's fun
    /*
    idx -= 1 * (idx == POINTS);
    idx += 1 * (idx == 0);
    */

    if (idx == POINTS) [[unlikely]] {
      i_a = idx - 2;
      i_b = idx - 1;
    } else if (idx == 0) [[unlikely]] {
      i_a = idx;
      i_b = idx + 1;
    } else {
      // linear between the points
      i_a = idx - 1;
      i_b = idx;
    }

    auto p_a = this->points[i_a];
    auto p_b = this->points[i_b];

    auto top = (p_a.y * (p_b.x - x)) + (p_b.y * (x - p_a.x));
    auto bottom = p_b.x - p_a.x;

    float nv = top / bottom;

#ifdef YAHP_DEBUG
    if (nv > 2) [[unlikely]] {
      Serial.printf("inner weird? nv %f, top %f, btm %f, pbx %f, pax %f,"
                    "pby %f, pay %f, x %f, i_a %d, i_b %d, pts %d\r\n",
                    nv, top, bottom, p_b.x, p_a.x, p_b.y, p_a.y, x, i_a, i_b,
                    this->points.size());
    }
#endif

    return nv;
  }

  // if it matches exactly, don't need to do any resampling/averaging
  interpolater_t(vector_t<point_t, MAGNET_INTERPOLATOR_POINTS> &inputs)
      : points(inputs) {}

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

template <uint32_t INPUT_POINTS, uint32_t OUTPUT_POINTS>
interpolater_t<OUTPUT_POINTS>
interpolater_from_many_points(vector_t<point_t, INPUT_POINTS> &inputs) {
  Serial.printf("Input number of points is %d\n\r", inputs.size());
  Serial.println("Going to sort");
  vector_t<point_t, OUTPUT_POINTS> outputs;

  // first, sort it by x
  for (uint32_t i = 0; i < inputs.size(); i++) {
    for (int32_t j = i + 1; j < (int32_t)inputs.size(); j++) {
      auto p_a = inputs[i];
      auto p_b = inputs[j];

      if (p_a.x > p_b.x) {
        inputs[i] = p_b;
        inputs[j] = p_a;
      }
    }
  }
  Serial.println("Sorted points");

  // now, split into ranges
  size_t stride = inputs.size() / OUTPUT_POINTS;

  Serial.printf("Found stride of %d\n\r", stride);

  for (size_t range_start = 0; range_start < inputs.size();
       range_start += stride) {
    size_t count = 0;
    float sum_x = 0;
    float sum_y = 0;
    for (size_t i = range_start; i < inputs.size() && i < range_start + stride;
         i++) {
      count++;
      auto p = inputs[i];
      sum_x += p.x;
      sum_y += p.y;
    }

    float x = sum_x / count;
    float y = sum_y / count;

    outputs.push_back({x, y});
  }
  Serial.println("Filling in with correct point count");

  return interpolater_t<OUTPUT_POINTS>(outputs);
}

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

static interpolater_t<MAGNET_INTERPOLATOR_POINTS> drop_test2(uint8_t bnum, uint8_t pin, bool fastcal) {
  set_board(bnum);
  delayMicroseconds(10);
  analogReadAveraging(16);
  analogReadResolution(10);
  pinMode(pin, INPUT_PULLDOWN);

  const int32_t CROSSING_COUNT = 2;

  float baseline = analogRead(pin);

  Serial.println("This step calibrates the native hammer range,");
  Serial.println("as well as the voltage response curve with respect to "
                 "distance of the sensors");

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

  bool above = false;
  uint32_t middle_crossings = 0;

  // how do we detect that the person has fully set the bounds?
  // is there a better way than just waiting for newline
  while (true) {
    float v = analogRead(pin);

    rough_max = max(rough_max, v);
    initial_resting = min(initial_resting, v);

    if (newline_waiting()) {
      break;
    }

    bool crosses = false;
    if (v > baseline + 30 && !above) {
      above = true;
      crosses = true;
    } else if (v < baseline && above) {
      above = false;
      crosses = true;
    }

    if (crosses) {
      int32_t remaining = CROSSING_COUNT - middle_crossings;
      if (remaining <= 0) {
        break;
      }
      middle_crossings++;
      Serial.printf("Crossed midpoint, remaining crosses: %d\r\n", remaining);
    }
  }

  Serial.printf("Found rough min %f and rough max %f\r\n", initial_resting,
                rough_max);
  if (!(fastcal || confirm("Is this acceptable?", true))) {
    Serial.println("Retrying...");
    return drop_test2(bnum, pin, fastcal);
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

  const int BUF_SIZE = 4096;
  vector_t<longsample_t, BUF_SIZE> buf;
  longsample_buf_t<BUF_SIZE> ring;

  while (true) {
    float v = analogRead(pin);
    if (v >= rough_max - 30) {
      break;
    }
  }

  Serial.println("Now, release the hammer and allow it to fall until it rests");
  auto start = micros();

  bool passed_midpoint = false;
  float midpoint = ((rough_max - initial_resting) / 2) + initial_resting;

  uint32_t midpoint_pos = 0;
  float slope_at_mid = 0;

  while(buf.size() < buf.max_size()) {
      if(passed_midpoint == true) {
          // check if we've settled at the bottom
          //
      } else {
      }
  }
}

static interpolater_t<MAGNET_INTERPOLATOR_POINTS>
drop_test(uint8_t bnum, uint8_t pin, bool fastcal) {
  set_board(bnum);
  delayMicroseconds(10);
  analogReadAveraging(16);
  analogReadResolution(10);
  pinMode(pin, INPUT_PULLDOWN);

  const int32_t CROSSING_COUNT = 2;

  float baseline = analogRead(pin);

  Serial.println("This step calibrates the native hammer range,");
  Serial.println("as well as the voltage response curve with respect to "
                 "distance of the sensors");

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

  bool above = false;
  uint32_t middle_crossings = 0;

  // how do we detect that the person has fully set the bounds?
  // is there a better way than just waiting for newline
  while (true) {
    float v = analogRead(pin);

    rough_max = max(rough_max, v);
    initial_resting = min(initial_resting, v);

    if (newline_waiting()) {
      break;
    }

    bool crosses = false;
    if (v > baseline + 30 && !above) {
      above = true;
      crosses = true;
    } else if (v < baseline && above) {
      above = false;
      crosses = true;
    }

    if (crosses) {
      int32_t remaining = CROSSING_COUNT - middle_crossings;
      if (remaining <= 0) {
        break;
      }
      middle_crossings++;
      Serial.printf("Crossed midpoint, remaining crosses: %d\r\n", remaining);
    }
  }

  Serial.printf("Found rough min %f and rough max %f\r\n", initial_resting,
                rough_max);
  if (!(fastcal || confirm("Is this acceptable?", true))) {
    Serial.println("Retrying...");
    return drop_test(bnum, pin, fastcal);
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
  const int BUF_SIZE = 4096;
  vector_t<longsample_t, BUF_SIZE> buf;

  // first, start sampling into a ring
  longsample_buf_t<2056> ring;

  // need to detect the first falling edge when dropping the hammer,
  // but don't want to accidentally trigger on noise,
  // so set a drop threshold

  while (true) {
    float v = analogRead(pin);
    if (v >= rough_max - 30) {
      break;
    }
  }

  Serial.println("Now, release the hammer and allow it to fall until it rests");
  auto start = micros();

  bool passed_midpoint = false;
  float midpoint = ((rough_max - initial_resting) / 2) + initial_resting;
  // until buf full
  uint32_t midpoint_pos = 0;
  while (buf.size() < buf.max_size()) {
    delayMicroseconds(500);
    float v = analogRead(pin);
    uint32_t t = micros() - start;
    longsample_t s(v, t);
    if (!passed_midpoint) {
      ring.add_sample(s);

      if (v < midpoint) {
        passed_midpoint = true;
        Serial.println("Just passed midpoint");
        // move them all over, do processing later
        for (int32_t i = ring.size - 1; i >= 0; i--) {
          if (buf.full()) {
            eloop("horrible state");
          }
          buf.push_back(ring.read_nth_oldest(i));
        }
        midpoint_pos = buf.size();
      }
    } else {
      buf.push_back(s);
      if(s.value < initial_resting + 10) {
          break;
      }
    }
  }

  Serial.printf("Gathered %d datapoints\r\n", buf.size());

  Serial.println("Data gathered, processing...");

  // probably won't take this many samples, but can still do

  uint32_t start_pos = 0;
  uint32_t end_pos = buf.size();

  for (int32_t i = midpoint_pos; i > 0; i--) {
    if (buf[i].value >= rough_max - 10) {
      start_pos = i;
      break;
    }
  }

  Serial.println("Got first bound, finding second...");

  for (int32_t i = midpoint_pos; i < (int32_t)buf.size(); i++) {
    if (buf[i].value <= initial_resting + 10) {
      end_pos = i;
      break;
    }
  }

  Serial.printf("Found endpoints: %d, %d\r\n", start_pos, end_pos);

  vector_t<longsample_t, BUF_SIZE> fall;

  // now, we have our bounds
  for (uint32_t i = start_pos; i <= end_pos; i++) {
    // backwards, because buf is a forgetful queue
    fall.push_back(buf[i]);
  }

  Serial.printf("Created fall buf with size %d\r\n", fall.size());

  // now, figure out what the points _should_ be
  // in terms of actual "distance" according to
  // laws of gravity

  Serial.println("Doing gravity simulation");

  float g = -9.81;
  float initial_velocity = 0;
  float initial_distance = 0;
  auto t_0 = fall.at(0).time;

  vector_t<point_t, BUF_SIZE> datapoints;

  // TODO: is this right? do I need to know if these are 0 vs uninit?
  float height_max = 0;
  float height_min = 999999; // eh

  float val_max = 0;
  float val_min = 999999;
  for (auto &sample : fall) {
    uint32_t t_i = sample.time - t_0;
    float t_if = (float)t_i / 1000000; // convert micros to seconds

    float height =
        0.5 * g * (t_if * t_if) + (initial_velocity * t_if) + initial_distance;

    point_t p((float)sample.value, height);

    datapoints.push_back(p);

    // only falling downward, I hope
    height_max = max(p.y, height_max);
    height_min = min(p.y, height_min);
    val_max = max(p.x, val_max);
    val_min = max(p.x, val_min);
  }

  Serial.printf("Height span is %f to %f mm\r\n", height_min * 1000,
                height_max * 1000);

  // Serial.printf("Created x-y dp vec with size %d\r\n", datapoints.size());

  // normalize distance to between 0 and 1,
  // 1 being the highest (closest to sensor), 0 being the "bottom"
  // the datapoints currently should have a max near 0,
  // and a min somewhere in the negatives
  float range = height_max - height_min;

  Serial.printf("Range is %f mm\r\n", range * 1000);
  Serial.println(
      "Gravity simulation complete, now normalizing points to within range...");
  for (auto &p : datapoints) {
    float cur_p = p.y;

    p.y = (cur_p - height_min) / (range);
  }

  // now we have a bunch of datapoints mapping flux density
  // to distance, and time is nowhere to be found
  Serial.println("Done normalizing, now creating interpolater from points...");

  // set up an interpolater that can reduce this large sample down to a small
  // number of control points
  auto itp =
      interpolater_from_many_points<BUF_SIZE, MAGNET_INTERPOLATOR_POINTS>(
          datapoints);

  Serial.println("Got output profile:");
  for (auto pt : itp.points) {
    Serial.printf("{ 'x': %f, 'y': %f },\r\n", pt.x, pt.y);
  }

  return itp;
}

#endif
