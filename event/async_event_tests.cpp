//
// Created by irantha on 5/14/23.
//

#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <thread>
#include <vector>

#include "async_spin_wait.hpp"
#include "task/async_task.hpp"
#include "task/sync_task.hpp"
#include "task/task.hpp"
#include "test/async_test_utils.hpp"
#include "async_event.hpp"
#include "sync_auto_reset_countdown_event.hpp"
#include "sync_auto_reset_event.hpp"
#include "sync_manual_reset_event.hpp"

namespace Levelz::Async::Test {

using namespace std::chrono_literals;

TEST_CASE("AsyncEvent - initially signal", "[AsyncEvent]")
{
    std::atomic<int> value = 0;
    AsyncEvent event { true };

    auto coroutine = [&value, &event]() -> Task<int> {
        value = 1;
        co_await event;
        value = 2;
        co_return -1;
    };

    auto runner = [&]() -> SyncTask<> {
        auto task = coroutine();
        REQUIRE(value == 0);
        auto returnValue = co_await task;
        REQUIRE(returnValue == -1);
        REQUIRE(value == 2);
    };
    runner().get();
}

TEST_CASE("AsyncEvent - initially not signal", "[AsyncEvent]")
{
    std::atomic<int> value = 0;
    AsyncEvent event { false };
    SyncAutoResetEvent syncEvent { false };

    auto run = [&]() -> SyncTask<int> {
        value++;
        syncEvent.set();
        co_await event;
        value++;
        syncEvent.set();
        co_return 0;
    };

    auto task = run();
    syncEvent.wait();
    REQUIRE(value == 1);
    event.signal();
    syncEvent.wait();
    REQUIRE(value == 2);
    REQUIRE(task.get() == 0);
}

TEST_CASE("AsyncEvent - multiple waiting coroutines", "[AsyncEvent]")
{
    REPEAT_HEADER

    constexpr int count = 100;
    std::atomic<int> value = 0;
    AsyncEvent event { false };
    SyncAutoResetEvent syncEvent { false };
    SyncAutoResetCountDownEvent syncCountdownEvent { count };

    auto run = [&]() -> SyncTask<int> {
        value++;
        syncEvent.set();
        co_await event;
        int tmp = ++value;
        syncCountdownEvent.set();
        co_return tmp;
    };

    std::vector<SyncTask<int>> tasks;
    tasks.reserve(count);
    for (int i = 0; i < count; i++) {
        tasks.push_back(run());
        syncEvent.wait();
        REQUIRE(value == i + 1);
    }
    REQUIRE(value == count);
    event.signal();
    syncCountdownEvent.wait();
    REQUIRE(value == count * 2);

    std::vector<int> values;
    values.reserve(count);
    for (int i = 0; i < count; i++) {
        values.push_back(tasks[i].get());
    }
    std::sort(values.begin(), values.end());
    for (int i = 0; i < count; i++) {
        REQUIRE(values[i] == count + 1 + i);
    }
    REQUIRE(values[count - 1] == 200);

    REPEAT_FOOTER
}

TEST_CASE("AsyncEvent - reset", "[AsyncEvent]")
{
    constexpr int count = 1000;
    std::atomic<int> value = 0;
    AsyncEvent event { false };
    SyncAutoResetCountDownEvent syncCountdownEvent { count };

    auto run = [&]() -> SyncTask<int> {
        value++;
        syncCountdownEvent.set();
        co_await event;
        int tmp = ++value;
        syncCountdownEvent.set();
        co_return tmp;
    };

    std::vector<SyncTask<int>> tasks;
    tasks.reserve(count);
    for (int i = 0; i < count; i++) {
        tasks.push_back(run());
    }
    syncCountdownEvent.wait();
    REQUIRE(value == count);
    event.signal();
    syncCountdownEvent.wait();
    REQUIRE(value == 2 * count);
    event.reset();
    for (int i = 0; i < count; i++) {
        tasks.push_back(run());
    }
    syncCountdownEvent.wait();
    REQUIRE(value == count * 3);
    event.signal();
    syncCountdownEvent.wait();
    REQUIRE(value == count * 4);
    for (auto& t : tasks)
        (void)t.get();
}

TEST_CASE("AsyncEvent - async enqueue event", "[AsyncEvent]")
{
    std::atomic<int> value = 0;
    AsyncEvent sourceEvent {};
    AsyncEvent targetEvent {};
    sourceEvent.enqueue(targetEvent);

    SyncAutoResetEvent syncEvent { false };

    auto run = [&]() -> SyncTask<int> {
        value++;
        syncEvent.set();
        co_await targetEvent;
        value++;
        syncEvent.set();
        co_return 0;
    };

    auto task = run();
    syncEvent.wait();
    REQUIRE(value == 1);
    sourceEvent.signal();
    syncEvent.wait();
    REQUIRE(value == 2);
    task.get();
}

TEST_CASE("AsyncEvent - async enqueue sync event", "[AsyncEvent]")
{
    std::atomic<int> value = 0;
    AsyncEvent sourceEvent {};
    SyncManualResetEvent targetEvent { false };
    sourceEvent.enqueue(targetEvent);

    SyncAutoResetEvent syncEvent { false };

    auto run = [&]() -> SyncTask<int> {
        value++;
        syncEvent.set();
        AsyncSpinWait spinWait;
        while (!targetEvent.isSet())
            spinWait.spinOne();
        value++;
        syncEvent.set();
        co_return 0;
    };

    auto task = run();
    syncEvent.wait();
    REQUIRE(value == 1);
    sourceEvent.signal();
    syncEvent.wait();
    REQUIRE(value == 2);
    task.get();
}

TEST_CASE("Immediately cancel Async awaiting on event", "[AsyncEvent]")
{
    REPEAT_HEADER
    AsyncEvent event;
    std::atomic<int> value2 = 0;
    std::atomic<int> value1 = 0;

    auto c2 = [&]() -> Async<> {
        value2++;
        co_await event;
        value2++;
    };

    auto c1 = [&]() -> Async<> {
        value1++;
        co_await event;
        value1++;
    };

    auto runner = [&]() -> Sync<> {
        auto task1 = c1();
        auto task2 = c2();

        AsyncSpinWait spinWait;
        while (event.waitListedCount() != 2)
            spinWait.spinOne();

        REQUIRE(value2 == 1);
        REQUIRE(value1 == 1);

        task2.cancel();

        REQUIRE(event.waitListedCount() == 1);
        REQUIRE_THROWS_AS(co_await task2, CancellationError);

        event.signal();

        co_await task1;

        REQUIRE(value1 == 2);
        REQUIRE(value2 == 1);
        co_return;
    };
    runner().get();
    REPEAT_FOOTER
}

}