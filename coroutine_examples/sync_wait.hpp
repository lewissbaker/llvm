#pragma once

#include <experimental/coroutine>
#include <mutex>
#include <condition_variable>
#include <utility>

#include "task.hpp"

struct sync_wait_task {
    struct promise_type {
        sync_wait_task get_return_object() noexcept {
            return sync_wait_task{
                std::experimental::coroutine_handle<promise_type>::from_promise(*this)};
        }

        std::experimental::suspend_always initial_suspend() {
            return {};
        }

        auto final_suspend() noexcept {
            struct awaiter {
                bool await_ready() { return false; }
                void await_suspend(std::experimental::coroutine_handle<promise_type> h) {
                    auto& promise = h.promise();
                    std::lock_guard lock{promise.mut_};
                    promise.done_ = true;
                    promise.cv_.notify_one();
                }
                void await_resume() {}
            };
            return awaiter{};
        }

        void return_void() noexcept {}

        void unhandled_exception() noexcept {
            error_ = std::current_exception();
        }

        void wait() {
            std::unique_lock lock{mut_};
            cv_.wait(lock, [&] { return done_; });

            if (error_) {
                std::rethrow_exception(error_);
            }
        }
    private:
        std::mutex mut_;
        std::condition_variable cv_;
        bool done_ = false;
        std::exception_ptr error_;
    };

    using handle_t = std::experimental::coroutine_handle<promise_type>;

    explicit sync_wait_task(handle_t coro) noexcept
    : coro_(coro) {}

    sync_wait_task(sync_wait_task&& t) noexcept
    : coro_(std::exchange(t.coro_, {}))
    {}

    ~sync_wait_task() {
        if (coro_) {
            coro_.destroy();
        }
    }

    void wait() {
        coro_.resume();
        coro_.promise().wait();
    }

private:
    handle_t coro_;
};

inline void sync_wait(task<void>&& t) {
    [&]() -> sync_wait_task {
        co_await std::move(t);
    }().wait();
}
