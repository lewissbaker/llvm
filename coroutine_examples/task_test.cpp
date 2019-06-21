#include "sync_wait.hpp"
#include "task.hpp"
#include "new_thread_executor.hpp"

#include <thread>
#include <chrono>

task<int> compute_meaning_of_life(new_thread_context::executor ex) {
    // Schedule this coroutine onto a new thread.
    co_await ex.schedule();

    using namespace std::chrono_literals;

    std::this_thread::sleep_for(1s);

    co_return 42;
}

task<void> run(new_thread_context::executor ex) {
    int result = co_await compute_meaning_of_life(ex);
    printf("meaning of life is %i\n", result);
}

int main() {
    new_thread_context context;
    sync_wait(run(context.get_executor()));
    return 0;
}
