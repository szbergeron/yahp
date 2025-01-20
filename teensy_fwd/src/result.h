#include "unit.h"
#include "utils.cpp"

#ifndef YAHP_RESULT
#define YAHP_RESULT

// I know this is dumb and broken but I'm just whatever
template<typename T, typename E>
struct result_t {
    private:
        union inner {
            T ok;
            E err;

            inner() {}
            ~inner() {}
        } inner;
        bool _is_ok;

        result_t(E e): _is_ok(false) {
            this->inner.err = e;
        }

        result_t(T v): _is_ok(true) {
            this->inner.ok = v;
        }

    public:
        static result_t<T, E> ok(T v) {
            return result_t(v);
        }

        static result_t<T, E> err(E e) {
            return result_t(e);
        }

        T unwrap() {
            if(!this->_is_ok) [[unlikely]] {
                eloop("tried to get an OK from an ERR");
            }

            return this->inner.ok;
        }

        T unwrap_or(T v) {
            if(!this->_is_ok) [[unlikely]] {
                return v;
            } else {
                return this->inner.ok;
            }
        }

        E unwrap_err() {
            if(this->_is_ok) [[unlikely]] {
                eloop("tried to get an ERR from an OK");
            }

            return this->inner.err;
        }

        bool is_ok() {
            return this->_is_ok;
        }
        
        ~result_t() {
            if (this->_is_ok) {
                (&this->inner.ok)->T::~T();
            } else {
                (&this->inner.err)->E::~E();
            }
        }
};

/*template<typename T>
using option_t = result_t<T, unit_t>;*/

#endif
