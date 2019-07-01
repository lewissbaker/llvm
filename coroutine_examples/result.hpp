#pragma once

#include <exception>
#include <cassert>
#include <utility>

#include "manual_lifetime.hpp"

template<typename T>
class result {
public:
    result() noexcept {}
    result(const result& other) {
        if (other.state_ == 1)
            emplace(other.value_);
        else if (other.state_ == 2)
            emplace_exception(other.error_);
    }
    result(result&& other) {
        if (other.state_ == 1)
            emplace(std::move(other.value_));
        else if (other.state_ == 2)
            emplace_exception(std::move(other.error_));
    }
    ~result() {
        reset();
    }
    template<typename... Args>
    void emplace(Args&&... args) {
        reset();
        value_.construct((Args&&)args...);
        state_ = 1;
    }
    void emplace_exception(std::exception_ptr e) {
        reset();
        error_.construct(std::move(e));
        state_ = 2;
    }

    bool valid() const noexcept { return state_ != 0; }
    bool has_value() const noexcept { return state_ == 1; }
    bool has_exception() const noexcept { return state_ == 2; }

    std::exception_ptr exception() const {
        assert(has_exception());
        return error_.get();
    }

    decltype(auto) value() & {
        rethrow_if_exception();
        return value_.get();
    }

    decltype(auto) value() const & {
        rethrow_if_exception();
        return value_.get();
    }

    decltype(auto) value() && {
        rethrow_if_exception();
        return std::move(value_).get();
    }

    decltype(auto) value() const && {
        rethrow_if_exception();
        return std::move(value_).get();
    }

    void reset() {
        if (state_ == 1) value_.destruct();
        else if (state_ == 2) error_.destruct();
        state_ = 0;
    }
private:
    void rethrow_if_exception() {
        if (state_ == 2) std::rethrow_exception(error_.get());
    }

    int state_ = 0;
    union {
        manual_lifetime<T> value_;
        manual_lifetime<std::exception_ptr> error_;
    };
};
