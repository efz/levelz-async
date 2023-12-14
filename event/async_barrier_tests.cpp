//
// Created by irantha on 6/8/23.
//

#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <vector>

#include "task/async_task.hpp"
#include "task/cancellation_error.hpp"
#include "task/sync_task.hpp"
#include "test/async_test_utils.hpp"
#include "async_barrier.hpp"

namespace Levelz::Async::Test {

TEST_CASE("AsyncBarrier", "[AsyncBarrier]")
{
    constexpr int rounds = 5;
    constexpr int workers = 3;
    REPEAT_HEADER

    int values[rounds * workers];
    for (int& value : values)
        value = -1;
    std::atomic<int> index = 0;
    AsyncBarrier barrier { workers };

    auto worker = [&]() -> Async<> {
        for (int i = 0; i < rounds; i++) {
            values[index++] = i;
            co_await barrier;
        }
    };

    auto run = [&]() -> SyncTask<> {
        std::vector<Async<>> tasks;
        tasks.reserve(workers);
        for (int i = 0; i < workers; i++)
            tasks.push_back(worker());

        for (int i = 0; i < workers; i++)
            co_await tasks[i];

        co_return;
    };

    run().get();
    for (int i = 0; i < rounds; i++)
        for (int j = 0; j < workers; j++)
            REQUIRE(values[i * workers + j] == i);

    REPEAT_FOOTER
}

TEST_CASE("AsyncBarrier - cancel", "[AsyncBarrier]")
{
    constexpr int rounds = 5;
    constexpr int workers = 3;
    REPEAT_HEADER

    int values[rounds * workers];
    for (int& value : values)
        value = -1;
    std::atomic<int> index = 0;
    AsyncBarrier barrier { workers };

    auto worker = [&]() -> Async<> {
        for (int i = 0; i < rounds; i++) {
            values[index++] = i;
            co_await barrier;
        }
    };

    auto run = [&]() -> SyncTask<> {
        std::vector<Async<>> tasks;
        tasks.reserve(workers);
        tasks.push_back(worker());

        REQUIRE_FALSE(barrier.isCanceled());
        barrier.cancel();
        REQUIRE(barrier.isCanceled());
        tasks.push_back(worker());

        REQUIRE_THROWS_AS(co_await tasks[0], CancellationError);
        REQUIRE_THROWS_AS(co_await tasks[1], CancellationError);
        REQUIRE(barrier.isWaitListEmpty());

        co_return;
    };

    run().get();
    REPEAT_FOOTER
}

TEST_CASE("AsyncBarrier - cancel task blocked at barrier", "[AsyncBarrier]")
{
    constexpr int workers = 3;
    REPEAT_HEADER

    int values[workers];
    for (int& value : values)
        value = -1;
    std::atomic<int> index = 0;
    AsyncBarrier barrier { workers + 1 };

    auto worker = [&](int i) -> Async<> {
        values[index++] = i;
        co_await barrier;
    };

    auto run = [&]() -> SyncTask<> {
        std::vector<Async<>> tasks;
        tasks.reserve(workers);
        for (int i = 0; i < workers; i++)
            tasks.push_back(worker(i));

        REQUIRE_FALSE(barrier.isCanceled());
        AsyncSpinWait asyncSpinWait;
        while (barrier.waitListedCount() != 3)
            asyncSpinWait.spinOne();

        tasks[0].cancel();

        REQUIRE_THROWS_AS(co_await tasks[0], CancellationError);
        REQUIRE_THROWS_AS(co_await tasks[1], CancellationError);
        REQUIRE_THROWS_AS(co_await tasks[2], CancellationError);
        REQUIRE(barrier.isWaitListEmpty());
        REQUIRE(barrier.isCanceled());

        co_return;
    };

    run().get();
    REPEAT_FOOTER
}

}
