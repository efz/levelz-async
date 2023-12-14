//
// Created by irantha on 6/3/23.
//

#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <thread>

#include "task/async_task.hpp"
#include "task/sync_task.hpp"
#include "test/async_test_utils.hpp"
#include "async_value.hpp"

namespace Levelz::Async::Test {

TEST_CASE("Shared AsyncValue - int shared", "[AsyncValue]")
{
    AsyncValue<int> asyncValue {};
    auto run = [&asyncValue]() -> SyncTask<int> {
        auto result = co_await asyncValue;
        co_return result;
    };

    auto task1 = run();
    auto task2 = run();

    asyncValue.setAndSignal(10);
    REQUIRE(task1.get() == 10);
    REQUIRE(task2.get() == 10);
}

TEST_CASE("Shared AsyncValue - int", "[AsyncValue]")
{
    std::atomic<int> value = 0;
    AsyncValue<int> asyncValue {};
    auto run = [&asyncValue, &value]() -> SyncTask<int> {
        value = 1;
        auto result = co_await asyncValue;
        value = 2;
        co_return result;
    };

    asyncValue.setAndSignal(10);
    REQUIRE(run().get() == 10);
    REQUIRE(value == 2);
}

TEST_CASE("Shared AsyncValue - void shared", "[AsyncValue]")
{
    int value = 0;
    AsyncValue<> asyncValue {};
    auto run = [&asyncValue, &value]() -> SyncTask<int> {
        co_await asyncValue;
        co_return value;
    };

    auto task1 = run();
    auto task2 = run();
    value = 10;
    asyncValue.setAndSignal();
    REQUIRE(task1.get() == 10);
    REQUIRE(task2.get() == 10);
}

TEST_CASE("Shared AsyncValue - int reference", "[AsyncValue]")
{
    int value = 0;
    AsyncValue<int&> asyncValue {};
    auto run = [&asyncValue](int* valuePtr) -> SyncTask<> {
        decltype(auto) result = co_await asyncValue;
        REQUIRE(result == 10);
        REQUIRE(&result == valuePtr);
        co_return;
    };

    auto task1 = run(&value);
    auto task2 = run(&value);

    value = 10;
    asyncValue.setAndSignal(value);
    task1.get();
    task2.get();
}

TEST_CASE("AsyncValue - int", "[AsyncValue]")
{
    std::atomic<int> value = 0;
    AsyncValue<int> asyncValue {};
    auto run = [&asyncValue, &value]() -> SyncTask<int> {
        value = 1;
        auto result = co_await asyncValue;
        value = 2;
        co_return result;
    };

    asyncValue.setAndSignal(10);
    REQUIRE(run().get() == 10);
    REQUIRE(value == 2);
}

TEST_CASE("AsyncValue - void", "[AsyncValue]")
{
    std::atomic<int> value = 0;
    AsyncValue<void> asyncValue {};
    auto run = [&asyncValue, &value]() -> SyncTask<int> {
        value = 1;
        co_await asyncValue;
        value = 2;
        co_return 5;
    };

    asyncValue.setAndSignal();
    REQUIRE(run().get() == 5);
    REQUIRE(value == 2);
}

TEST_CASE("AsyncValue - reference", "[AsyncValue]")
{
    int value = 0;
    AsyncValue<int&> asyncValue {};
    auto run = [&asyncValue]() -> SyncTask<> {
        decltype(auto) ref = co_await asyncValue;
        REQUIRE(ref == 0);
        ref = 2;
        co_return;
    };

    asyncValue.setAndSignal(value);
    run().get();
    REQUIRE(value == 2);
}

TEST_CASE("Immediately cancel Async awaiting on async value", "[AsyncValue]")
{
    REPEAT_HEADER
    AsyncValue<> asyncValue;
    std::atomic<int> value2 = 0;
    std::atomic<int> value1 = 0;

    auto c2 = [&]() -> Async<> {
        value2++;
        co_await asyncValue;
        value2++;
    };

    auto c1 = [&]() -> Async<> {
        value1++;
        co_await asyncValue;
        value1++;
    };

    auto runner = [&]() -> Sync<> {
        auto task1 = c1();
        auto task2 = c2();

        AsyncSpinWait spinWait;
        while (asyncValue.waitListedCount() != 2)
            spinWait.spinOne();

        REQUIRE(value2 == 1);
        REQUIRE(value1 == 1);

        task2.cancel();

        REQUIRE(asyncValue.waitListedCount() == 1);
        REQUIRE_THROWS_AS(co_await task2, CancellationError);

        asyncValue.setAndSignal();

        co_await task1;

        REQUIRE(value1 == 2);
        REQUIRE(value2 == 1);
        co_return;
    };
    runner().get();
    REPEAT_FOOTER
}

}