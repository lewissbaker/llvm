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

    task<T> get_return_object() noexcept;

    std::experimental::suspend_always initial_suspend() {
        return {};
    }

    auto final_suspend() noexcept {
        struct awaiter {
            bool await_ready() noexcept { return false; }
            auto await_suspend(std::experimental::coroutine_handle<task_promise> h) noexcept {
                return h.promise().continuation_;
            }
            void await_resume() noexcept {}
        };
        return awaiter{};
    }

    template<
        typename U, 
        std::enable_if_t<std::is_convertible_v<U, T>, int> = 0>
    void return_value(U&& value) {
        clear();
        value_.construct((U&&)value);
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

    std::experimental::coroutine_handle<> continuation_;
    enum class state_t { empty, value, error };
    state_t state_ = state_t::empty;
    union {
        manual_lifetime<T> value_;
        manual_lifetime<std::exception_ptr> error_;
    };
};

template<>
class task_promise<void> {
public:
    task_promise() noexcept {}

    ~task_promise() {
        clear();
    }

    task<void> get_return_object() noexcept;

    std::experimental::suspend_always initial_suspend() {
        return {};
    }

    auto final_suspend() {
        struct awaiter {
            bool await_ready() { return false; }
            auto await_suspend(std::experimental::coroutine_handle<task_promise> h) {
                return h.promise().continuation_;
            }
            void await_resume() {}
        };
        return awaiter{};
    }

    void return_void() {
        clear();
        value_.construct();
        state_ = state_t::value;
    }

    void unhandled_exception() noexcept {
        clear();
        error_.construct(std::current_exception());
        state_ = state_t::error;
    }

    void get() {
        if (state_ == state_t::error) {
            std::rethrow_exception(std::move(error_).get());
        }
    }

private:
    friend class task<void>;

    void clear() noexcept {
        switch (std::exchange(state_, state_t::empty)) {
        case state_t::empty: break;
        case state_t::error: error_.destruct(); break;
        case state_t::value: value_.destruct(); break;
        }
    }

    std::experimental::coroutine_handle<> continuation_;
    enum class state_t { empty, value, error };
    state_t state_ = state_t::empty;
    union {
        manual_lifetime<void> value_;
        manual_lifetime<std::exception_ptr> error_;
    };
};

template<typename T>
class task {
public:
    using promise_type = task_promise<T>;
    using handle_t = std::experimental::coroutine_handle<promise_type>;

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

    auto operator co_await() && noexcept {
        struct awaiter {
        public:
            explicit awaiter(handle_t coro) : coro_(coro) {}
            bool await_ready() noexcept {
                return false;
            }
            auto await_suspend(std::experimental::coroutine_handle<> h) noexcept {
                coro_.promise().continuation_ = h;
                return coro_;
            }
            T await_resume() {
                return coro_.promise().get();
            }
        private:
            handle_t coro_;
        };
        return awaiter{coro_};
    }

private:
    handle_t coro_;
};

template<typename T>
task<T> task_promise<T>::get_return_object() noexcept {
    return task<T>{
        std::experimental::coroutine_handle<task_promise<T>>::from_promise(*this)
        };
}

inline
task<void> task_promise<void>::get_return_object() noexcept {
    return task<void>{
        std::experimental::coroutine_handle<task_promise<void>>::from_promise(*this)
        };
}