#pragma once

#include <experimental/coroutine>
#include <type_traits>

template <typename T>
using _is_continuation_handle =
    std::is_convertible<T, std::experimental::continuation_handle>;

template <typename T>
struct _is_valid_await_suspend_return_type : std::disjunction<
                                                 std::is_void<T>,
                                                 std::is_same<bool, T>,
                                                 _is_continuation_handle<T>> {};

template <typename T, typename = void>
struct is_awaiter : std::false_type {};

template <typename T>
struct is_awaiter<
    T,
    std::void_t<
        decltype(std::declval<T&>().await_ready()),
        decltype(std::declval<T&>().await_suspend(
            std::declval<std::experimental::suspend_point_handle<
                std::experimental::with_resume>>())),
        decltype(std::declval<T&>().await_resume())>>
    : std::conjunction<
          std::is_same<bool, decltype(std::declval<T&>().await_ready())>,
          _is_valid_await_suspend_return_type<decltype(
              std::declval<T&>().await_suspend(
                  std::declval<std::experimental::suspend_point_handle<
                      std::experimental::with_resume>>()))>> {};

template <typename T>
constexpr bool is_awaiter_v = is_awaiter<T>::value;


template <typename Awaitable, typename = void>
struct _has_member_operator_co_await : std::false_type {};

template <typename Awaitable>
struct _has_member_operator_co_await<
    Awaitable,
    std::void_t<decltype(std::declval<Awaitable>().operator co_await())>>
    : is_awaiter<decltype(std::declval<Awaitable>().operator co_await())> {};

template <typename Awaitable, typename = void>
struct _has_free_operator_co_await : std::false_type {};

template <typename Awaitable>
struct _has_free_operator_co_await<
    Awaitable,
    std::void_t<decltype(operator co_await(std::declval<Awaitable>()))>>
    : is_awaiter<decltype(operator co_await(std::declval<Awaitable>()))> {};

template <typename T>
struct is_awaitable : std::disjunction<
                          _has_member_operator_co_await<T>,
                          _has_free_operator_co_await<T>,
                          is_awaiter<T>> {};

template <typename T>
constexpr bool is_awaitable_v = is_awaitable<T>::value;

template <
    typename Awaitable,
    std::enable_if_t<
        std::conjunction<
            is_awaiter<Awaitable>,
            std::negation<_has_free_operator_co_await<Awaitable>>,
            std::negation<_has_member_operator_co_await<Awaitable>>>::
            value,
        int> = 0>
Awaitable& get_awaiter(Awaitable&& awaitable) {
  return awaitable;
}

template <
    typename Awaitable,
    std::enable_if_t<
        _has_member_operator_co_await<Awaitable>::value,
        int> = 0>
decltype(auto) get_awaiter(Awaitable&& awaitable) {
  return static_cast<Awaitable&&>(awaitable).operator co_await();
}

template <
    typename Awaitable,
    std::enable_if_t<
        std::conjunction<
            _has_free_operator_co_await<Awaitable>,
            std::negation<_has_member_operator_co_await<Awaitable>>>::
            value,
        int> = 0>
decltype(auto) get_awaiter(Awaitable&& awaitable) {
  return operator co_await(static_cast<Awaitable&&>(awaitable));
}

template <typename Awaitable, typename = void>
struct awaiter_type {};

template <typename Awaitable>
struct awaiter_type<Awaitable, std::enable_if_t<is_awaitable_v<Awaitable>>> {
  using type = decltype(get_awaiter(std::declval<Awaitable>()));
};

template <typename Awaitable>
using awaiter_type_t = typename awaiter_type<Awaitable>::type;

template <typename Awaitable, typename = void>
struct await_result {};

template <typename Awaitable>
struct await_result<Awaitable, std::enable_if_t<is_awaitable_v<Awaitable>>> {
  using type =
      decltype(std::declval<awaiter_type_t<Awaitable>&>().await_resume());
};

template <typename Awaitable>
using await_result_t = typename await_result<Awaitable>::type;
