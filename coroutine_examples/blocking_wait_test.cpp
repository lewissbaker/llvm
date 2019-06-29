#include <experimental/coroutine>
#include <type_traits>
#include <mutex>
#include <condition_variable>

#include <stdio.h>

#include "traits.hpp"
#include "result.hpp"

template <typename T>
class blocking_wait_task
{
public:
    class promise_type
    {
    public:
        promise_type() noexcept = default;
        ~promise_type() = default;

        template<typename SuspendPointHandle>
        blocking_wait_task get_return_object(SuspendPointHandle sp) noexcept {
            return blocking_wait_task{sp};
        }

        auto done() noexcept {
            std::lock_guard lock{mut_};
            cv_.notify_one();
            return std::experimental::noop_continuation();
        }

        void unhandled_exception() noexcept {
            result_->emplace_exception(std::current_exception());
        }

        template <
            typename U,
            std::enable_if_t<std::is_convertible<U, T>::value, int> = 0>
        void return_value(U&& value) noexcept(
            std::is_nothrow_constructible<T, U&&>::value) {
            assert(result_ != nullptr);
            result_->emplace(static_cast<U&&>(value));
        }

        template <typename U = T, std::enable_if_t<std::is_void_v<U>, int> = 0>
        void return_void() noexcept {
            assert(result_ != nullptr);
            result_->emplace();
        }

        void set_result_ptr(result<T>* r) {
            result_ = r;
        }

        void wait() {
            std::unique_lock lock{mut_};
            cv_.wait(lock, [this] { return result_->valid(); });
        }
    private:
        result<T>* result_;
        std::mutex mut_;
        std::condition_variable cv_;
    };

    using handle_t = std::experimental::suspend_point_handle<
        std::experimental::with_resume,
        std::experimental::with_destroy,
        std::experimental::with_promise<promise_type>>;

    explicit blocking_wait_task(handle_t coro) noexcept
    : coro_(coro) {}

    blocking_wait_task(blocking_wait_task&& t)
    : coro_(std::exchange(t.coro_, {}))
    {}

    ~blocking_wait_task() {
        if (coro_) {
            coro_.destroy();
        }
    }

    T get() && {
        auto& promise = coro_.promise();
        result<T> x;
        promise.set_result_ptr(&x);
        coro_.resume()();
        promise.wait();
        return std::move(x).value();
    }
private:
    handle_t coro_;
};

template <typename Awaitable>
auto make_blocking_wait_task(Awaitable&& awaitable) -> blocking_wait_task<await_result_t<Awaitable>> {
  co_return co_await static_cast<Awaitable&&>(awaitable);
}

template <typename Awaitable>
auto blocking_wait(Awaitable&& awaitable) -> await_result_t<Awaitable> {
  return make_blocking_wait_task(static_cast<Awaitable&&>(awaitable)).get();
}

struct simple_awaitable {
    bool await_ready() { return true; }
    template<typename Handle>
    void await_suspend(Handle h) {
    }
    int await_resume() { return 42; }
};

int main() {
    int result = blocking_wait(simple_awaitable{});
    printf("result = %i\n", result);
    return 0;
}
