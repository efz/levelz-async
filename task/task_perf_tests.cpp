//
// Created by irantha on 6/16/23.
//

#ifndef LEVELZ_TASK_PERF_TESTS_CPP
#define LEVELZ_TASK_PERF_TESTS_CPP

#include <catch2/catch_test_macros.hpp>
#include <iostream>
#include <unordered_map>

#include "event/async_mutex.hpp"
#include "task/async_task.hpp"
#include "task/sync_task.hpp"
#include "test/async_test_utils.hpp"

namespace Levelz::Async::Test {

namespace {
    struct DirectFibonacci {
        DirectFibonacci()
            : m_valueMap {}
        {
        }

        uint64_t directCompute(uint64_t n)
        {
            if (m_valueMap.contains(n))
                return m_valueMap[n];

            uint64_t result;
            if (n == 0 || n == 1) {
                result = 1;
            } else {
                uint64_t fn_1 = directCompute(n - 1);
                uint64_t fn_2 = directCompute(n - 2);
                result = fn_1 + fn_2;
            }
            m_valueMap[n] = result;
            return result;
        }

    private:
        std::unordered_map<uint64_t, uint64_t> m_valueMap;
    };

    struct AsyncFibonacci {
        AsyncFibonacci()
            : m_tasksMap {}
            , m_mutex {}
        {
        }

        Async<uint64_t> asyncCompute(uint64_t n)
        {
            if (n == 0 || n == 1)
                co_return 1;

            auto fn_1 = asyncComputeOrGet(n - 1);
            auto fn_2 = asyncComputeOrGet(n - 2);

            co_return co_await fn_1 + co_await fn_2;
        }

        void cancel()
        {
            m_mutex.cancel();
        }

    private:
        Async<uint64_t> asyncComputeOrGet(uint64_t n)
        {
            Async<uint64_t> task;
            {
                auto lock = co_await m_mutex;
                (void)lock;

                if (!m_tasksMap.contains(n))
                    m_tasksMap[n] = asyncCompute(n);

                task = m_tasksMap[n];
            }
            co_return co_await task;
        }

        std::unordered_map<uint64_t, Async<uint64_t>> m_tasksMap;
        AsyncMutex m_mutex;
    };

    TEST_CASE("Async Fibonacci - perf benchmark", "[Task]")
    {
        constexpr uint64_t N = 50;
        constexpr uint64_t R = 20365011074;

        AsyncFibonacci asyncFibonacci {};

        auto runAsync = [&](uint64_t n) -> SyncTask<uint64_t> {
            auto result = co_await asyncFibonacci.asyncCompute(n);
            co_return result;
        };

        {
            using clock = std::chrono::high_resolution_clock;
            auto start = clock::now();
            auto r = runAsync(N).get();
            auto end = clock::now();
            std::chrono::duration<double, std::micro> d = end - start;
            std::cout << "Async duration: " << d.count() << std::endl;
            REQUIRE(r == R);
        }
        REQUIRE(1 == 1);

        DirectFibonacci directFibonacci {};
        {
            using clock = std::chrono::high_resolution_clock;
            auto start = clock::now();
            auto r = directFibonacci.directCompute(N);
            auto end = clock::now();
            std::chrono::duration<double, std::micro> d = end - start;
            std::cout << "Direct duration: " << d.count() << std::endl;
            REQUIRE(r == R);
        }
        REQUIRE(1 == 1);
    }

    TEST_CASE("Async Fibonacci - shutdown", "[Task]")
    {
        constexpr uint64_t N = 1000;
        uint64_t cancelledCount = 0;

        REPEAT_HEADER

        auto runner = [&](uint64_t n) -> SyncTask<uint64_t> {
            AsyncFibonacci asyncFibonacci {};
            auto task = asyncFibonacci.asyncCompute(n);
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(50us);
            asyncFibonacci.cancel();
            co_return co_await task;
        };

        auto runnerTask = runner(N);
        try {
            auto result = runnerTask.get();
            (void)result;
        } catch (const CancellationError&) {
            cancelledCount++;
        }

        REPEAT_FOOTER
        REQUIRE(cancelledCount > REPEAT_COUNT * 0.9);
    }
}

}

#endif // LEVELZ_TASK_PERF_TESTS_CPP
