#ifndef YAHP_VECTOR
#define YAHP_VECTOR

#include "ArduinoJson/Array/JsonArrayConst.hpp"
#include "ArduinoJson/Document/JsonDocument.hpp"
#include "ArduinoJson/Variant/JsonVariantConst.hpp"
#include "result.h"
#include "unit.h"
#include "utils.cpp"
#include <cstddef>

template <typename T> class vector_iterator_t {
public:
  vector_iterator_t(T *values_ptr) : values_ptr_{values_ptr}, position_{0} {}

  vector_iterator_t(T *values_ptr, size_t size)
      : values_ptr_{values_ptr}, position_{size} {}

  bool operator!=(const vector_iterator_t<T> &other) const {
    return !(*this == other);
  }

  bool operator==(const vector_iterator_t<T> &other) const {
    return position_ == other.position_;
  }

  vector_iterator_t &operator++() {
    ++position_;
    return *this;
  }

  T &operator*() const { return *(values_ptr_ + position_); }

private:
  T *values_ptr_;
  size_t position_ = 0;
};

template <typename T, size_t MAX_LEN> struct vector_t {
private:
  union elem {
    T some;
    unit_t none;

    elem() : none() {}

    ~elem() {}
  };

  elem elements[MAX_LEN];
  size_t cur_len = 0;

public:
  typedef vector_iterator_t<T> iterator;

  bool push_back(T v) {
    if (this->cur_len >= MAX_LEN) [[unlikely]] {
      Serial.println("exceeded vec len");
      return false;
    } else {
      this->elements[this->cur_len].some = v;
      this->cur_len++;

      return true;
    }
  }

  T &operator[](size_t idx) { return this->at(idx); }

  /*result_t<T&, unit_t> at(size_t idx) {
  }*/

  T &at(size_t idx) {
    if (idx >= this->cur_len) [[unlikely]] {
      Serial.printf("Tried to index to %d within a vec of size %d\r\n", idx,
                    this->cur_len);
      Serial.printf("capacity is %d\r\n", MAX_LEN);
      eloop("idx out of range");
    }

    return this->elements[idx].some;
  }

  result_t<T*, unit_t> find_by(std::function<bool(const T&)> filter) {
      for(auto& v: *this) {
          if(filter(v)) {
              return result_t<T*, unit_t>::ok(&v);
          }
      }

      return result_t<T*, unit_t>::err({});
  }

  T& find_by_or_insert(std::function<bool(const T&)> filter, const T& insert) {
      auto r = this->find_by(filter);
      if(r.is_ok()) {
          return *r.unwrap();
      } else {
          this->push_back(insert);
          return this->elements[this->size() - 1].some;
      }
  }

  bool contains(const T &v) {
      for(auto &elem : *this) {
          if(elem == v) {
              return true;
          }
      }

      return false;
  }

  void clear() {
      while(!this->empty()) {
          this->pop_back();
      }
  }

  size_t size() { return this->cur_len; }

  size_t max_size() { return MAX_LEN; }

  bool empty() { return this->size() == 0; }

  bool full() { return this->size() >= this->max_size() - 1; }

  T &back() { return this->at(this->cur_len - 1); }

  T &start() { return this->at(0); }

  iterator begin() { return iterator(reinterpret_cast<T *>(this->elements)); }

  iterator end() {
    return iterator(reinterpret_cast<T *>(this->elements), this->cur_len);
  }

  result_t<T, unit_t> pop_back() {
    if (this->cur_len > 0) {
      this->cur_len--;

      T v = this->elements[this->cur_len].some;

      return result_t<T, unit_t>::ok(v);
    } else {
      return result_t<T, unit_t>::err({});
    }
  }

  ~vector_t() {
    for (size_t i = 0; i < this->cur_len; i++) {
      (&this->elements[i].some)->T::~T();
    }
  }

  JsonDocument prim_to_json() {
    JsonDocument di;

    auto a = di.to<JsonArray>();

    for (auto ent : *this) {
      a.add(ent);
    }

    return di;
  }

  JsonDocument to_json() {
    JsonDocument di;

    auto a = di.to<JsonArray>();

    for (auto ent : *this) {
      a.add(ent.to_json());
    }

    return di;
  }

  vector_t() : cur_len(0) {}

  vector_t(JsonArray ja) : cur_len(0) {
    for (auto ent : ja) {
      auto v = T(ent);
      this->push_back(v);
    }
  }

  vector_t(JsonArrayConst ja) : cur_len(0) {
    for (auto ent : ja) {
      auto v = T::from_json(ent);
      this->push_back(v);
    }
  }
};

/*template<size_t MAX_LEN>
class vector_t<float, MAX_LEN> {
    JsonDocument to_json() {
    }
};*/

#endif
