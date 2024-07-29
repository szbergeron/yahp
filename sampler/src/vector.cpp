#ifndef YAHP_VECTOR
#define YAHP_VECTOR

#include <cstddef>
#include "utils.cpp"
#include "unit.h"

template<typename T, size_t MAX_LEN>
struct vector_t {
    private:
        union elem {
            T some;
            unit_t none;
        };

        elem elements[MAX_LEN];
        size_t cur_len;

    public:
        bool push_back(T v) {
            if(this->cur_len >= MAX_LEN) [[unlikely]] {
                Serial.println("exceeded vec len");
                return false;
            } else {
                this->cur_len++;

                this->elements[this->cur_len].some = v;
            }
        }

        size_t size() {
            return this->cur_len;
        }

        size_t max_size() {
            return MAX_LEN;
        }



        result_t<T, unit_t> pop_back() {
        }
};

template<typename T>
class vector_iterator_t
{
public:
  vector_iterator_t(T * values_ptr) : values_ptr_{values_ptr}, position_{0} {}

  vector_iterator_t(T * values_ptr, size_t size) : values_ptr_{values_ptr}, position_{size} {}

  bool operator!=(const vector_iterator_t<T> & other) const
  {
    return !(*this == other);
  }

  bool operator==(const vector_iterator_t<T> & other) const
  {
    return position_ == other.position_;
  }

  vector_iterator_t & operator++()
  {
    ++position_;
    return *this;
  }

  T & operator*() const
  {
    return *(values_ptr_ + position_);
  }

private:
  T * values_ptr_;
  size_t position_;
};

#endif
