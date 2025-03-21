#ifndef YAHP_VECTOR
#define YAHP_VECTOR

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
  size_t position_;
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
  size_t cur_len;

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

  size_t size() { return this->cur_len; }

  size_t max_size() { return MAX_LEN; }

  bool empty() { return this->size() == 0; }

  bool full() { return this->size() == MAX_LEN; }

  T &back() { return this->at(this->cur_len - 1); }

  T &start() { return this->at(0); }

  T* data() { return reinterpret_cast<T *>(this->elements); }

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

  vector_t() : cur_len(0) {}
};

#endif
