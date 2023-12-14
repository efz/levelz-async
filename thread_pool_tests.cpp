//
// Created by irantha on 5/4/23.
//

#include <catch2/catch_test_macros.hpp>
#include <iostream>
#include <thread>
#include <vector>

#include "task/async_task.hpp"
#include "task/sync_task.hpp"
#include "task/task.hpp"
#include "test/async_test_utils.hpp"
#include "thread_pool.hpp"

namespace Levelz::Async::Test {

TEST_CASE("ThreadPool - construct/destruct", "[ThreadPool]")
{
    ThreadPool threadPool { 5, ThreadPoolKind::Default };
    REQUIRE(threadPool.threadCount() == std::min(5, ThreadPool::maxThreadCount()));
}

TEST_CASE("ThreadPool - one task", "[ThreadPool]")
{
    auto initialThreadId = std::this_thread::get_id();
    auto run = [=]() -> Sync<> {
        if (std::this_thread::get_id() == initialThreadId)
            FAIL();
        co_return;
    };
    run().get();
}

TEST_CASE("ThreadPool - async task thread pool switching", "[ThreadPool]")
{
    auto bgTask = []() -> BackgroundAsync<bool> {
        co_return ThreadPool::currentThreadPoolKind() == ThreadPoolKind::Background;
    };

    auto fgTask = []() -> DefaultAsync<bool> {
        co_return ThreadPool::currentThreadPoolKind() == ThreadPoolKind::Default;
    };

    auto cTask = []() -> Async<bool> {
        co_return ThreadPool::currentThreadPoolKind() == ThreadPoolKind::Default;
    };

    auto run = [&]() -> SyncTask<> {
        REQUIRE(co_await bgTask());
        REQUIRE(co_await cTask());
        REQUIRE(co_await fgTask());
        co_return;
    };
    run().get();
    REQUIRE(ThreadPool::currentThreadPoolKind() == ThreadPoolKind::Current);
}

TEST_CASE("ThreadPool - task thread pool switching", "[ThreadPool]")
{
    auto bgTask = []() -> BackgroundTask<bool> {
        co_return ThreadPool::currentThreadPoolKind() == ThreadPoolKind::Background;
    };

    auto fgTask = []() -> DefaultTask<bool> {
        co_return ThreadPool::currentThreadPoolKind() == ThreadPoolKind::Default;
    };

    auto cTask = []() -> Task<bool> {
        co_return ThreadPool::currentThreadPoolKind() == ThreadPoolKind::Default;
    };

    auto run = [&]() -> SyncTask<> {
        REQUIRE(co_await bgTask());
        REQUIRE(co_await cTask());
        REQUIRE(co_await fgTask());
        co_return;
    };
    run().get();
    REQUIRE(ThreadPool::currentThreadPoolKind() == ThreadPoolKind::Current);
}

TEST_CASE("ThreadPool - sync task thread pool switching", "[ThreadPool]")
{
    auto bgTask = []() -> BackgroundSync<bool> {
        co_return ThreadPool::currentThreadPoolKind() == ThreadPoolKind::Background;
    };

    auto fgTask = []() -> DefaultSync<bool> {
        co_return ThreadPool::currentThreadPoolKind() == ThreadPoolKind::Default;
    };

    auto cTask = []() -> Sync<bool> {
        co_return ThreadPool::currentThreadPoolKind() == ThreadPoolKind::Default;
    };

    REQUIRE(bgTask().get());
    REQUIRE(cTask().get());
    REQUIRE(fgTask().get());

    REQUIRE(ThreadPool::currentThreadPoolKind() == ThreadPoolKind::Current);
}

TEST_CASE("ThreadPool - many tasks", "[ThreadPool]")
{
    std::vector<Async<>> tasks;
    constexpr int taskCount = 100;
    std::atomic<int> value;

    auto task = [&]() -> Async<> {
        AsyncTestUtils::randomSpinWait(100);
        value++;
        co_return;
    };

    auto runner = [&]() -> Sync<> {
        for (int ii = 0; ii < taskCount; ii++)
            tasks.push_back(task());
        co_return;
    };

    runner().get();
    REQUIRE(tasks.size() == taskCount);
    REQUIRE(value == taskCount);
}

}
