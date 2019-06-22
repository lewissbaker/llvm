#pragma once

#include <experimental/coroutine>
#include <iterator>
#include <cassert>

#include "manual_lifetime.hpp"

template<typename Ref, typename Value = std::decay_t<Ref>>
class generator {
public:
    class iterator;

    class promise_type {
    public:
        friend iterator;

        promise_type() noexcept {}

        ~promise_type() noexcept {
            clear_value();
        }

        void clear_value() {
            if (hasValue_) {
                hasValue_ = false;
                ref_.destruct();
            }
        }

        template<typename SuspendPointHandle>
        generator get_return_object(SuspendPointHandle sp) noexcept {
            sp_ = sp;
            return generator{sp};
        }

        auto done() noexcept {
            return std::experimental::noop_continuation();
        }

        struct yield_awaiter {
            bool await_ready() noexcept { return false; }
            template<typename SuspendPointHandle>
            auto await_suspend(SuspendPointHandle sp) noexcept {
                sp.promise().sp_ = sp;
                return std::experimental::noop_continuation();
            }
            void await_resume() noexcept {}
        };

        yield_awaiter yield_value(Ref ref)
                noexcept(std::is_nothrow_move_constructible_v<Ref>) {
            ref_.construct(std::move(ref));
            return {};
        }

        void return_void() {
            sp_ = {};
        }

        void unhandled_exception() {
            sp_ = {};
            throw;
        }

        Ref get() {
            return ref_.get();
        }

    private:
        std::experimental::suspend_point_handle<
            std::experimental::with_resume,
            std::experimental::with_set_done> sp_;
        manual_lifetime<Ref> ref_;
        bool hasValue_ = false;
    };

    using handle_t = std::experimental::suspend_point_handle<
        std::experimental::with_resume,
        std::experimental::with_destroy,
        std::experimental::with_promise<promise_type>>;

    generator(generator&& g) noexcept
    : coro_(std::exchange(g.coro_, {}))
    {}

    ~generator() {
        if (coro_) {
            coro_.destroy();
        }
    }

    struct sentinel {};

    class iterator {
    public:
        using reference = Ref;
        using value_type = Value;
        using distance_type = size_t;
        using pointer = std::add_pointer_t<Ref>;
        using iterator_category = std::input_iterator_tag;

        iterator() noexcept {}

        explicit iterator(promise_type& promise) noexcept
        : promise_(promise) {}

        reference operator*() const {
            return promise_.get();
        }

        iterator& operator++() {
            promise_.clear_value();
            promise_.sp_.resume()();
            return *this;
        }

        void operator++(int) {
            promise_.clear_value();
            promise_.sp_.resume()();
        }

        bool operator==(sentinel) const noexcept {
            return !promise_.sp_;
        }

        bool operator!=(sentinel s) const noexcept {
            return !operator==(s);
        }

    private:
        promise_type& promise_;
    };

    iterator begin() {
        coro_.resume()();
        return iterator{coro_.promise()};
    }

    sentinel end() {
        return {};
    }

private:

    explicit generator(handle_t coro) noexcept
    : coro_(coro) {}

    handle_t coro_;
};
