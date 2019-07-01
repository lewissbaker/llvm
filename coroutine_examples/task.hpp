#pragma once

#include <experimental/coroutine>
#include <type_traits>
#include <utility>

#include "manual_lifetime.hpp"

template<typename T>
class task;

template<typename T>
class task_promise {
public:
    task_promise() noexcept {}

    ~task_promise() {
        clear();
    }

    template<typename Handle>
    task<T> get_return_object(Handle h) noexcept;

    auto done() noexcept {
        return continuation_;
    }

    template<
        typename U, 
        std::enable_if_t<std::is_convertible_v<U, T>, int> = 0>
    void return_value(U&& value) {
        clear();
        value_.construct((U&&)value);
        state_ = state_t::value;
    }

    template<
        typename U = T,
        std::enable_if_t<std::is_void_v<U>, int> = 0>
    void return_void() noexcept {
        clear();
        value_.construct();
        state_ = state_t::value;
    }

    void unhandled_exception() noexcept {
        clear();
        error_.construct(std::current_exception());
        state_ = state_t::error;
    }

    T get() {
        if (state_ == state_t::error) {
            std::rethrow_exception(std::move(error_).get());
        }
        return std::move(value_).get();
    }

private:
    friend class task<T>;

    void clear() noexcept {
        switch (std::exchange(state_, state_t::empty)) {
        case state_t::empty: break;
        case state_t::error: error_.destruct(); break;
        case state_t::value: value_.destruct(); break;
        }
    }

    std::experimental::continuation_handle continuation_;
    enum class state_t { empty, value, error };
    state_t state_ = state_t::empty;
    union {
        manual_lifetime<T> value_;
        manual_lifetime<std::exception_ptr> error_;
    };
};


template<typename T>
class task {
public:
    using promise_type = task_promise<T>;
    using handle_t = std::experimental::suspend_point_handle<
        std::experimental::with_resume,
        std::experimental::with_destroy,
        std::experimental::with_promise<promise_type>>;

    explicit task(handle_t h) noexcept
    : coro_(h)
    {}

    task(task&& t) noexcept
    : coro_(std::exchange(t.coro_, {}))
    {}

    ~task() {
        if (coro_) {
            coro_.destroy();
        }
    }

private:
    struct awaiter {
    public:
        explicit awaiter(handle_t coro) : coro_(coro) {}
        bool await_ready() noexcept {
            return false;
        }
        template<typename Handle>
        auto await_suspend(Handle h) noexcept {
            coro_.promise().continuation_ = h.resume();
            return coro_.resume();
        }
        T await_resume() {
            return coro_.promise().get();
        }
    private:
        handle_t coro_;
    };

public:

    auto operator co_await() && noexcept {
        return awaiter{coro_};
    }

private:
    handle_t coro_;
};

template<typename T>
template<typename Handle>
task<T> task_promise<T>::get_return_object(Handle h) noexcept {
    return task<T>{h};
}
