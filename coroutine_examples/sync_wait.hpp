#pragma once

#include <experimental/coroutine>
#include <mutex>
#include <condition_variable>
#include <utility>

#include "task.hpp"

struct sync_wait_task {
    struct promise_type {
        template<typename Handle>
        sync_wait_task get_return_object(Handle h) noexcept {
            return sync_wait_task{h};
        }

        auto done() noexcept {
            std::lock_guard lock{mut_};
            done_ = true;
            cv_.notify_one();
            return std::experimental::noop_continuation();
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

    using handle_t = std::experimental::suspend_point_handle<
        std::experimental::with_resume,
        std::experimental::with_destroy,
        std::experimental::with_promise<promise_type>>;

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
        coro_.resume()();
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
