//
// Created by irantha on 6/5/23.
//

#include <catch2/catch_test_macros.hpp>
#include <vector>

#include "task/async_task.hpp"
#include "task/sync_task.hpp"
#include "test/async_test_utils.hpp"
#include "async_mutex.hpp"
#include "test/assertion_error.hpp"

namespace Levelz::Async::Test {

namespace {
    using AssertionError = Levelz::Async::Test::AssertionError;
}

TEST_CASE("AsyncMutex - lock unlock", "[AsyncMutex]")
{
    AsyncMutex mu;
    int result = 0;

    auto coroutine = [&]() -> Async<int> {
        auto lock = co_await mu;
        (void)lock;
        result++;
        co_return result;
    };

    auto run = [&]() -> SyncTask<int> {
        co_return co_await coroutine();
    };

    REPEAT_HEADER
    for (int i = 0; i < 100; i++) {
        REQUIRE(run().get() == i + 1);
    }
    REQUIRE(result == 100);
    result = 0;
    REPEAT_FOOTER
}

TEST_CASE("AsyncMutex - concurrency", "[AsyncMutex]")
{
    REPEAT_HEADER
    AsyncMutex mu;
    int account1 = 100;
    int account2 = 100;

    auto transfer = [&](int amount) -> Async<> {
        auto lock = co_await mu;
        (void)lock;
        if (account1 + account2 != 200)
            throw AssertionError("requirement failed");
        if (amount > 0) {
            account1 += amount;
            account2 -= amount;
        } else {
            account1 -= amount;
            account2 += amount;
        }
        if (account1 + account2 != 200)
            throw AssertionError("requirement failed");
    };

    auto run = [&]() -> SyncTask<void> {
        for (int i = 0; i < 100; i++) {
            std::vector<Async<>> tasks;
            tasks.push_back(transfer(10));
            tasks.push_back(transfer(1));
            tasks.push_back(transfer(-7));
            tasks.push_back(transfer(-7));
            tasks.push_back(transfer(8));
            tasks.push_back(transfer(-50));
            tasks.push_back(transfer(-20));
            tasks.push_back(transfer(3));
            tasks.push_back(transfer(33));
            tasks.push_back(transfer(0));
            for (auto& task : tasks)
                co_await task;

            REQUIRE(account1 + account2 == 200);
            REQUIRE(account1 != account2);
        }

        REQUIRE(account1 + account2 == 200);
        REQUIRE(account1 != account2);
    };
    run().get();
    REPEAT_FOOTER
}

TEST_CASE("Immediately cancel Async awaiting on mutex", "[AsyncMutex]")
{
    REPEAT_HEADER
    AsyncEvent event;
    AsyncMutex mutex;
    std::atomic<int> values[] = { 0, 0 };

    auto c2 = [&]() -> Async<> {
        values[1]++;
        auto lock = co_await mutex;
        (void)lock;
        values[1]++;
        co_await event;
        values[1]++;
    };

    auto c1 = [&]() -> Async<> {
        values[0]++;
        auto lock = co_await mutex;
        (void)lock;
        values[0]++;
        co_await event;
        values[0]++;
    };

    auto runner = [&]() -> Sync<> {
        Async<> tasks[] = { c1(), c2() };

        AsyncSpinWait spinWait;
        while (mutex.waitListedCount() != 1)
            spinWait.spinOne();

        while (event.waitListedCount() != 1)
            spinWait.spinOne();

        auto mutexAcquiredIndex = values[0] == 2 ? 0 : 1;
        auto mutexNotAcquiredIndex = values[0] == 2 ? 1 : 0;

        REQUIRE(values[mutexAcquiredIndex] == 2);
        REQUIRE(values[mutexNotAcquiredIndex] == 1);

        tasks[mutexNotAcquiredIndex].cancel();

        REQUIRE(mutex.waitListedCount() == 0);

        REQUIRE_THROWS_AS(co_await tasks[mutexNotAcquiredIndex], CancellationError);

        event.signal();

        co_await tasks[mutexAcquiredIndex];

        REQUIRE(values[mutexAcquiredIndex] == 3);
        REQUIRE(values[mutexNotAcquiredIndex] == 1);
        co_return;
    };
    runner().get();
    REPEAT_FOOTER
}

}
