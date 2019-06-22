#pragma once

#include <mutex>
#include <thread>
#include <condition_variable>
#include <experimental/coroutine>

class new_thread_context {
public:

    new_thread_context() {}

    ~new_thread_context() {
        std::unique_lock lock{mut_};
        cv_.wait(lock, [&] { return activeThreadCount_ == 0; });
    }

private:
    class schedule_op {
    public:
        explicit schedule_op(new_thread_context& context)
        : context_(context)
        {}

        bool await_ready() { return false; }

        template<typename SuspendPointHandle>
        void await_suspend(SuspendPointHandle h) {
            {
                std::lock_guard lock{context_.mut_};
                ++context_.activeThreadCount_;
            }

            try {
                std::thread{[this, h]() mutable {
                    // Resume the coroutine.
                    h.resume()();

                    std::unique_lock lock{context_.mut_};
                    --context_.activeThreadCount_;
                    std::notify_all_at_thread_exit(context_.cv_, std::move(lock));
                }}.detach();
            } catch (...) {
                std::lock_guard lock{context_.mut_};
                --context_.activeThreadCount_;
                throw;
            }
        }

        void await_resume() {}

    private:
        new_thread_context& context_;
    };

public:

    struct executor {
    public:
        explicit executor(new_thread_context& context) noexcept
        : context_(context)
        {}

        schedule_op schedule() noexcept {
            return schedule_op{context_};
        }

    private:
        new_thread_context& context_;
    };

    executor get_executor() { return executor{*this}; }

private:
    std::mutex mut_;
    std::condition_variable cv_;
    std::size_t activeThreadCount_ = 0;
};