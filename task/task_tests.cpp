//
// Created by irantha on 5/7/23.
//

#include <catch2/catch_test_macros.hpp>
#include <iostream>
#include <thread>
#include <vector>

#include "task/async_task.hpp"
#include "task/sync_task.hpp"
#include "task/task.hpp"
#include "test/async_test_utils.hpp"
#include "test/counted.hpp"

namespace Levelz::Async::Test {

TEST_CASE("Task - starts", "[Task]")
{
    std::atomic<bool> started = false;
    auto coroutine = [&]() -> Task<> {
        started = true;
        co_return;
    };

    auto run = [&]() -> SyncTask<> {
        REQUIRE(!started);
        auto task = coroutine();
        REQUIRE(!started);
        co_await task;
        REQUIRE(started);
    };
    run().get();
}

TEST_CASE("Task - starts on co_await", "[Task]")
{
    std::atomic<bool> started = false;
    std::atomic<bool> finished = false;
    auto coroutine = [&]() -> Task<> {
        started = true;
        co_return;
    };

    auto coroutine2 = [&]() -> Task<> {
        co_await coroutine();
        finished = true;
        co_return;
    };

    auto run = [&]() -> SyncTask<> {
        REQUIRE(!started);
        auto task = coroutine2();
        REQUIRE(!started);
        co_await task;
        REQUIRE(started);
        REQUIRE(finished);
    };
    run().get();
}

TEST_CASE("Task - return value", "[Task]")
{
    auto adder = [](int x, int y) -> Task<int> {
        co_return (x + y);
    };

    auto multiplier = [](int x, int y) -> Task<int> {
        co_return (x * y);
    };

    auto addMul = [adder, multiplier](int x, int y, int z) -> Task<int> {
        int r1 = co_await adder(x, y);
        int r2 = co_await multiplier(r1, z);
        co_return r2;
    };

    auto run = [&]() -> SyncTask<> {
        int result = co_await addMul(2, 3, 4);
        REQUIRE(result == 20);
    };

    run().get();
}

TEST_CASE("Task - lots of synchronous completions", "[Task]")
{
#ifdef DEBUG
    constexpr int count = 10'000;
#else
    constexpr int count = 1'000'000;
#endif

    auto completesSynchronously = []() -> Task<int> {
        co_return 1;
    };

    auto task = [&]() -> Task<int> {
        int sum = 0;
        for (int i = 0; i < count; ++i) {
            sum += co_await completesSynchronously();
        }
        co_return sum;
    };

    auto run = [&]() -> SyncTask<> {
        auto sum = co_await task();
        REQUIRE(sum == count);
    };
    run().get();
}

TEST_CASE("Task - destroying task that was never awaited destroys captured args", "[Task]")
{
    Counted::resetCounts();

    auto f = [](Counted c) -> Task<Counted> {
        co_return c;
    };

    REQUIRE(Counted::activeCount() == 0);

    {
        auto t = f(Counted {});
        REQUIRE(Counted::activeCount() == 1);
    }

    REQUIRE(Counted::activeCount() == 0);
}

TEST_CASE("Task - dependency free yield suspended task is sync destroyed", "[Task]")
{
    Counted::resetCounts();

    auto f = [](Counted c) -> Task<Counted> {
        co_yield c;
        co_yield Counted {};
        co_return Counted {};
    };

    REQUIRE(Counted::activeCount() == 0);

    auto runner = [&]() -> Sync<> {
        auto t = f(Counted {});
        auto r = co_await t;
        (void)r;
        REQUIRE(Counted::activeCount() == 4);
    };
    runner().get();

    REQUIRE(Counted::activeCount() == 0);
}

TEST_CASE("Task - coroutine method", "[Task]")
{
    struct CoroutineStruct {
    public:
        CoroutineStruct(int x, int y)
            : m_x { x }
            , m_y { y }
        {
        }

        Task<int> calc(int z) const
        {
            co_return m_x + m_y - z;
        }

    private:
        int m_x;
        int m_y;
    };

    auto task = []() -> Task<int> {
        CoroutineStruct crs { 4, 3 };
        auto val = co_await crs.calc(5);
        co_return val + 10;
    };

    auto run = [&]() -> SyncTask<> {
        auto ret = co_await task();
        REQUIRE(ret == 12);
    };
    run().get();
}

TEST_CASE("Task - reference type return value - 1", "[Task]")
{
    int value = 3;
    auto f = [&]() -> Task<int&> {
        co_return value;
    };

    auto task = [&]() -> Task<int&> {
        int& result = co_await f();
        static_assert(std::is_same<decltype(result), int&>::value);
        REQUIRE(&result == &value);
        co_return result;
    };

    auto run = [&]() -> SyncTask<> {
        decltype(auto) ret = co_await task();
        REQUIRE(&ret == &value);
    };
    run().get();
}

TEST_CASE("Task - reference type return value - 2", "[Task]")
{
    int value = 3;
    auto f = [&]() -> Task<int&> {
        co_return value;
    };

    auto task = [&]() -> Task<int&> {
        auto t = f();
        int& result = co_await t;
        static_assert(std::is_same<decltype(result), int&>::value);
        REQUIRE(&result == &value);
        co_return result;
    };

    auto run = [&]() -> SyncTask<> {
        decltype(auto) ret = co_await task();
        REQUIRE(&ret == &value);
    };
    run().get();
}

TEST_CASE("Task - pass by value", "[Task]")
{
    Counted::resetCounts();

    auto f = [](Counted arg) -> Task<void> {
        co_return;
    };

    Counted c;

    REQUIRE(Counted::activeCount() == 1);
    REQUIRE(Counted::s_defaultConstructionCount == 1);
    REQUIRE(Counted::s_copyConstructionCount == 0);
    REQUIRE(Counted::s_moveConstructionCount == 0);
    REQUIRE(Counted::s_destructionCount == 0);

    {
        auto t = f(c);
        REQUIRE(Counted::s_copyConstructionCount == 1);
        REQUIRE(Counted::activeCount() == 2);
    }
    REQUIRE(Counted::activeCount() == 1);
}

TEST_CASE("Task - sync task", "[Task]")
{
    auto adder = [](int x, int y) -> Task<int> {
        co_return (x + y);
    };

    auto multiplier = [](int x, int y) -> Task<int> {
        co_return (x * y);
    };

    auto addMul = [adder, multiplier](int x, int y, int z) -> SyncTask<int> {
        int r1 = co_await adder(x, y);
        int r2 = co_await multiplier(r1, z);
        co_return r2;
    };
    auto task = addMul(2, 3, 4);
    auto result = task.get();
    REQUIRE(result == 20);
}

TEST_CASE("Task - multiple sync tasks", "[Task]")
{
    auto adder = [](int x, int y) -> Task<int> {
        co_return (x + y);
    };

    auto multiplier = [](int x, int y) -> Task<int> {
        co_return (x * y);
    };

    auto addMul = [adder, multiplier](int x, int y, int z) -> SyncTask<int> {
        int r1 = co_await adder(x, y);
        int r2 = co_await multiplier(r1, z);
        co_return r2;
    };
    std::vector<SyncTask<int>> tasks;
    for (int i = 0; i < 1000; i++) {
        auto task = addMul(1, 1, 1);
        tasks.push_back(std::move(task));
    }
    for (auto& task : tasks) {
        auto result = task.get();
        REQUIRE(result == 2);
    }
}

namespace {
    uint64_t finbonacci(uint64_t n)
    {
        if (n == 0 || n == 1)
            return 1;

        uint64_t fn_1 = finbonacci(n - 1);
        uint64_t fn_2 = finbonacci(n - 2);
        return fn_1 + fn_2;
    }

    Task<uint64_t> finbonacciSync(uint64_t n, uint64_t limit)
    {
        if (n < limit)
            co_return finbonacci(n);

        uint64_t fn_1 = co_await finbonacciSync(n - 1, limit);
        uint64_t fn_2 = co_await finbonacciSync(n - 2, limit);
        co_return fn_1 + fn_2;
    }

    Async<uint64_t> finbonacciAsync(uint64_t n, uint64_t limit)
    {
        if (n < limit)
            co_return finbonacci(n);
        auto fn_1 = finbonacciAsync(n - 1, limit);
        auto fn_2 = finbonacciAsync(n - 2, limit);
        co_return co_await fn_1 + co_await fn_2;
    }
}

TEST_CASE("Task - fibonacci benchmark", "[Task]")
{
    constexpr uint64_t N = 40;
    constexpr uint64_t L = 25;
    constexpr uint64_t R = 165580141;
    auto run = [](uint64_t n) -> SyncTask<uint64_t> {
        using clock = std::chrono::high_resolution_clock;
        auto start = clock::now();
        auto result = co_await finbonacciSync(n, L);
        auto end = clock::now();
        std::chrono::duration<double, std::micro> d = end - start;
        std::cout << "sync duration: " << d.count() << std::endl;
        co_return result;
    };

    {

        using clock = std::chrono::high_resolution_clock;
        auto start = clock::now();
        auto r = run(N).get();
        auto end = clock::now();
        std::chrono::duration<double, std::micro> d = end - start;
        std::cout << "end-to-end sync duration: " << d.count() << std::endl;
        REQUIRE(r == R);
    }

    {
        using clock = std::chrono::high_resolution_clock;
        auto start = clock::now();
        auto r = finbonacci(N);
        auto end = clock::now();
        std::chrono::duration<double, std::micro> d = end - start;
        std::cout << "no coroutine Duration: " << d.count() << std::endl;
        REQUIRE(r == R);
    }

    auto runAsync = [](uint64_t n) -> SyncTask<uint64_t> {
        using clock = std::chrono::high_resolution_clock;
        auto start = clock::now();
        auto result = co_await finbonacciAsync(n, L);
        auto end = clock::now();
        std::chrono::duration<double, std::micro> d = end - start;
        std::cout << "async duration: " << d.count() << std::endl;
        co_return result;
    };

    {
        using clock = std::chrono::high_resolution_clock;
        auto start = clock::now();
        auto r = runAsync(N).get();
        auto end = clock::now();
        std::chrono::duration<double, std::micro> d = end - start;
        std::cout << "end-to-end async duration: " << d.count() << std::endl;
        REQUIRE(r == R);
    }
}

TEST_CASE("Task - async task", "[AsyncTask]")
{
    auto adder = [](int x, int y) -> Async<int> {
        co_return (x + y);
    };

    auto multiplier = [](int x, int y) -> Async<int> {
        co_return (x * y);
    };

    auto addMul = [adder, multiplier](int x, int y, int z) -> SyncTask<int> {
        int r1 = co_await adder(x, y);
        int r2 = co_await multiplier(r1, z);
        co_return r2;
    };
    auto task = addMul(2, 3, 4);
    auto result = task.get();
    REQUIRE(result == 20);
}

TEST_CASE("SharedTask int - multiple awaiters on async task", "[SharedTask]")
{
    REPEAT_HEADER
    AsyncEvent event;

    auto producer = [&]() -> Async<int> {
        co_await event;
        co_return 1;
    };

    auto consumer = [](Async<int>& t) -> Async<int> {
        auto value = co_await t;
        co_return value + 1;
    };

    auto p = producer();

    auto run = [&]() -> SyncTask<int> {
        auto c1 = consumer(p);
        auto c2 = consumer(p);
        auto c3 = consumer(p);
        event.signal();
        co_return co_await c1 + co_await c2 + co_await c3;
    };

    REQUIRE(run().get() == 6);
    REPEAT_FOOTER
}

TEST_CASE("SharedTask void - multiple awaiters", "[SharedTask]")
{
    REPEAT_HEADER
    AsyncEvent event;
    int value = 0;
    auto producer = [&]() -> Async<> {
        co_await event;
        value = 11;
        co_return;
    };

    auto consumer = [&value](Async<>& t) -> Async<int> {
        co_await t;
        co_return value + 1;
    };

    auto p = producer();

    auto run = [&]() -> SyncTask<int> {
        auto c1 = consumer(p);
        auto c2 = consumer(p);
        auto c3 = consumer(p);
        event.signal();
        auto r1 = co_await c1;
        auto r2 = co_await c2;
        auto r3 = co_await c3;
        co_return r1 + r2 + r3;
    };

    REQUIRE(run().get() == 36);
    REPEAT_FOOTER
}

namespace {
    template <template <typename> typename TaskType>
    auto valueFunc = [](int x) -> TaskType<int> {
        if (x == 0)
            co_return 1;
        else
            throw std::runtime_error { "error" };
    };

    template <template <typename> typename TaskType>
    auto valueFuncRunner = []() -> Sync<> {
        auto task1 = valueFunc<TaskType>(1);
        REQUIRE_THROWS_AS(co_await task1, std::runtime_error);
        auto task2 = valueFunc<TaskType>(0);
        auto result = co_await task2;
        REQUIRE(result == 1);
    };

    template <template <typename> typename TaskType>
    auto voidFunc = [](int x) -> TaskType<void> {
        if (x == 0)
            co_return;
        else
            throw std::runtime_error { "error" };
    };

    template <template <typename> typename TaskType>
    auto voidFuncRunner = []() -> Sync<> {
        auto task1 = valueFunc<TaskType>(1);
        REQUIRE_THROWS_AS(co_await task1, std::runtime_error);
        auto task2 = valueFunc<TaskType>(0);
        co_await task2;
    };

    template <template <typename> typename TaskType>
    auto refFunc = [](int& x) -> TaskType<int&> {
        if (x == 0)
            co_return x;
        else
            throw std::runtime_error { "error" };
    };

    template <template <typename> typename TaskType>
    auto refFuncRunner = []() -> Sync<> {
        int x = 1;
        auto task1 = refFunc<TaskType>(x);
        REQUIRE_THROWS_AS(co_await task1, std::runtime_error);
        int y = 0;
        auto task2 = refFunc<TaskType>(y);
        int& result = co_await task2;
        REQUIRE(result == y);
        REQUIRE(&result == &y);
    };
}

TEST_CASE("Task exception result", "[Task][Async][Shared][Sync]")
{
    SECTION("int result Task")
    {
        valueFuncRunner<DefaultTask>().get();
    }

    SECTION("int result Async")
    {
        valueFuncRunner<Async>().get();
    }

    SECTION("int result Shared")
    {
        valueFuncRunner<Async>().get();
    }

    SECTION("int result Sync")
    {
        auto task1 = valueFunc<Sync>(1);
        REQUIRE_THROWS_AS(task1.get(), std::runtime_error);
        auto task2 = valueFunc<Sync>(0);
        REQUIRE(task2.get() == 1);
    }

    SECTION("void result Task")
    {
        voidFuncRunner<DefaultTask>().get();
    }

    SECTION("void result Async")
    {
        voidFuncRunner<Async>().get();
    }

    SECTION("void result Shared")
    {
        voidFuncRunner<Async>().get();
    }

    SECTION("void result Sync")
    {
        auto task1 = voidFunc<Sync>(1);
        REQUIRE_THROWS_AS(task1.get(), std::runtime_error);
        auto task2 = voidFunc<Sync>(0);
        task2.get();
    }

    SECTION("ref result Task")
    {
        refFuncRunner<DefaultTask>().get();
    }

    SECTION("ref result Async")
    {
        refFuncRunner<Async>().get();
    }

    SECTION("ref result Shared")
    {
        refFuncRunner<Async>().get();
    }
}

TEST_CASE("Task - return unique_ptr", "[Task]")
{
    bool started = false;
    auto coroutine = [&]() -> Task<std::unique_ptr<Counted>&&> {
        started = true;
        REQUIRE(Counted::activeCount() == 0);
        co_return std::make_unique<Counted>();
    };

    auto run = [&]() -> SyncTask<> {
        REQUIRE(!started);
        REQUIRE(Counted::activeCount() == 0);
        auto task = coroutine();
        REQUIRE(!started);
        REQUIRE(Counted::activeCount() == 0);
        auto ptr = co_await task;
        REQUIRE(ptr);
        REQUIRE(started);
        REQUIRE(Counted::activeCount() == 1);
    };
    run().get();
    REQUIRE(Counted::activeCount() == 0);
}

TEST_CASE("Task - unique_ptr return type task that throws", "[Task]")
{
    bool started = false;
    auto coroutine = [&](bool shouldThrow) -> Task<std::unique_ptr<Counted>&&> {
        started = true;
        REQUIRE(Counted::activeCount() == 0);
        auto ptr = std::make_unique<Counted>();
        if (shouldThrow)
            throw std::runtime_error("error");
        co_return ptr;
    };

    auto run = [&]() -> SyncTask<> {
        REQUIRE(!started);
        REQUIRE(Counted::activeCount() == 0);
        auto task = coroutine(true);
        REQUIRE(!started);
        REQUIRE(Counted::activeCount() == 0);
        REQUIRE_THROWS_AS(co_await task, std::runtime_error);
        REQUIRE(started);
        REQUIRE(Counted::activeCount() == 0);
    };
    run().get();
    REQUIRE(Counted::activeCount() == 0);
}

TEST_CASE("SyncTask - SyncTask waits for AsyncTasks", "[SyncTask]")
{
    int completedCount = 0;
    int notStartedCount = 0;
    REPEAT_HEADER
    std::atomic<int> value = 0;

    auto tester = [&]() -> Async<> {
        value = 10;
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(200ns);
        value = 101;
        co_return;
    };

    auto runner = [&]() -> SyncTask<> {
        auto task = tester();
        AsyncTestUtils::randomSpinWait(100);
        co_return;
    };
    auto runnerTask = runner();
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(50ns);
    runnerTask.get();
    if (value == 0)
        notStartedCount++;
    else if (value == 101)
        completedCount++;
    REPEAT_FOOTER

    REQUIRE(completedCount > 0);
    REQUIRE(completedCount + notStartedCount == REPEAT_COUNT);
}

TEST_CASE("Task - Task waits for AsyncTasks", "[Task]")
{
    int completedCount = 0;
    int notStartedCount = 0;
    REPEAT_HEADER
    std::atomic<int> value { 0 };

    auto tester = [&]() -> Async<> {
        value = 10;
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(200ns);
        value = 101;
        co_return;
    };

    auto task = [&]() -> Task<> {
        auto task = tester();
        AsyncTestUtils::randomSpinWait(10);
        co_return;
    };

    auto runner = [&]() -> SyncTask<> {
        co_await task();
        co_return;
    };

    auto runnerTask = runner();
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(50ns);
    runnerTask.get();
    if (value == 0)
        notStartedCount++;
    else if (value == 101)
        completedCount++;
    REPEAT_FOOTER

    REQUIRE(completedCount > 0);
    REQUIRE(completedCount + notStartedCount == REPEAT_COUNT);
}

TEST_CASE("Task - Async return from Task", "[Task]")
{
    REPEAT_HEADER
    std::atomic<int> value[] { 0, 0 };

    auto tester = [&](int index) -> Async<> {
        value[index] = 10;
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(200ns);
        value[index] = 101;
        co_return;
    };

    auto task = [&](const Async<>& t) -> Task<Async<>> {
        auto t2 = tester(1);
        AsyncTestUtils::randomSpinWait(10);
        co_return t;
    };

    auto runner = [&]() -> SyncTask<> {
        Async<> task1 = tester(0);
        Async<> task2 = co_await task(task1);
        co_return;
    };

    auto runnerTask = runner();
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(50ns);
    runnerTask.get();

    REQUIRE(value[0] != 10);
    REQUIRE(value[1] != 10);
    REPEAT_FOOTER
}

TEST_CASE("Task - Async return from & pass as param", "[AsyncTask]")
{
    REPEAT_HEADER
    std::atomic<int> value[] { 0, 0 };

    auto tester = [&](int index) -> Async<> {
        value[index] = 10;
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(200ns);
        value[index] = 101;
        co_return;
    };

    auto task = [&](Async<> t) -> Async<std::pair<Async<>, Async<>>> {
        auto t2 = tester(1);
        AsyncTestUtils::randomSpinWait(10);
        co_return { t, t2 };
    };

    auto runner = [&]() -> SyncTask<> {
        Async<> task1 = tester(0);
        auto [t1, t2] = co_await task(task1);
        (void)t1;
        (void)t2;
        co_return;
    };

    auto runnerTask = runner();
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(10ns);
    runnerTask.get();

    REQUIRE(value[0] != 10);
    REQUIRE(value[1] != 10);
    REPEAT_FOOTER
}

TEST_CASE("SyncTask - reference type return value", "[SyncTask]")
{
    int value = 3;

    auto task = [&]() -> SyncTask<int&> {
        int& result = value;
        static_assert(std::is_same<decltype(result), int&>::value);
        REQUIRE(&result == &value);
        co_return result;
    };

    auto& ret = task().get();
    REQUIRE(&ret == &value);
}

TEST_CASE("SyncTask - r-value reference return value", "[SyncTask]")
{
    auto task = [&]() -> SyncTask<std::unique_ptr<int>&&> {
        auto result = std::make_unique<int>(3);
        co_return std::move(result);
    };

    auto ret = task().get();
    REQUIRE(*ret == 3);
}

}
