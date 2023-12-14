//
// Created by irantha on 6/22/23.
//

#include <catch2/catch_test_macros.hpp>

#include "async_spin_wait.hpp"
#include "event/async_barrier.hpp"
#include "event/async_event.hpp"
#include "event/async_mutex.hpp"
#include "task/async_task.hpp"
#include "task/cancellation_error.hpp"
#include "task/sync_task.hpp"
#include "task/task.hpp"
#include "test/async_test_utils.hpp"
#include "test/counted.hpp"

namespace Levelz::Async::Test {

TEST_CASE("Cancel - cancel Async await on event", "[AsyncTask]")
{
    REPEAT_HEADER
    AsyncEvent event1;
    AsyncEvent event2;
    std::atomic<int> value = 0;

    auto coroutine = [&]() -> Async<> {
        value++;
        event2.signal();
        co_await event1;
        value++;
    };

    auto runner = [&]() -> Sync<> {
        REQUIRE(value == 0);
        auto task = coroutine();
        co_await event2;
        REQUIRE(value == 1);
        task.cancel();
        event1.signal();
        REQUIRE_THROWS_AS(co_await task, CancellationError);
        REQUIRE(value == 1);
    };
    runner().get();
    REPEAT_FOOTER
}

TEST_CASE("Cancel - cancel Async await on Async", "[AsyncTask]")
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
    auto task2 = c2();
    auto c1 = [&]() -> Async<> {
        value1++;
        co_await task2;
        value1++;
    };

    auto runner = [&]() -> Sync<> {
        auto task1 = c1();
        AsyncSpinWait spinWait;
        while (value2 == 0 || value1 == 0) {
            spinWait.spinOne();
        }
        REQUIRE(value2 == 1);
        REQUIRE(value1 == 1);
        task2.cancel();
        event.signal();
        REQUIRE_THROWS_AS(co_await task1, CancellationError);
        REQUIRE(value1 == 1);
        REQUIRE(value2 == 1);
        co_return;
    };
    runner().get();
    REPEAT_FOOTER
}

TEST_CASE("Cancel - destroy event while task awaiting", "[AsyncEvent]")
{
    REPEAT_HEADER
    auto coroutine = [](auto* event, auto& value) -> Async<> {
        value++;
        co_await *event;
        value++;
    };

    auto runner = [&]() -> Sync<> {
        int value = 0;
        auto event = std::make_unique<AsyncEvent>();
        auto task = coroutine(event.get(), value);

        AsyncSpinWait spinWait;
        while (event->isWaitListEmpty() || value == 0) {
            spinWait.spinOne();
        }

        REQUIRE(value == 1);
        event.reset();
        REQUIRE(value == 1);
        REQUIRE_THROWS_AS(co_await task, CancellationError);
        co_return;
    };
    runner().get();
    REPEAT_FOOTER
}

TEST_CASE("Cancel - abandon Async", "[AsyncTask]")
{
    REPEAT_HEADER
    AsyncEvent event;
    std::atomic<int> value = 0;

    auto coroutine = [&](Counted counted) -> Async<Counted> {
        value++;
        co_await event;
        value++;
        co_return counted;
    };

    auto runner = [&]() -> Sync<> {
        REQUIRE(Counted::activeCount() == 0);
        {
            auto task = coroutine(Counted {});
            task.setCancelAbandoned(false);
            REQUIRE(Counted::activeCount() == 1);
            AsyncSpinWait spinWait;
            while (event.isWaitListEmpty() || value == 0)
                spinWait.spinOne();
        }
        REQUIRE(value.load() == 1);
        REQUIRE(Counted::activeCount() == 1);
        event.signal();

        AsyncSpinWait spinWait;
        while (!event.isWaitListEmpty() || Counted::activeCount() != 0)
            spinWait.spinOne();

        REQUIRE(value.load() == 2);
        co_return;
    };
    runner().get();
    REPEAT_FOOTER
}

TEST_CASE("Cancel - Mutex", "[AsyncTask]")
{
    REPEAT_HEADER
    AsyncMutex mutex;
    AsyncEvent event1;
    AsyncEvent event2;
    std::atomic<int> value1 = 0;
    std::atomic<int> value2 = 0;
    std::atomic<Async<>*> taskPtr;

    auto coroutine = [&](bool useValue1, bool cancel) -> Async<> {
        co_await event1;
        if (cancel)
            taskPtr.load()->cancel();
        auto lock = co_await mutex;
        (void)lock;
        if (useValue1)
            value1++;
        else
            value2++;
        co_await event2;
        if (useValue1)
            value1++;
        else
            value2++;
    };

    SECTION("awaiting to acquire")
    {
        auto runner = [&]() -> Sync<> {
            event1.signal();

            auto task1 = coroutine(true, false);
            AsyncSpinWait spinWait2;
            while (!mutex.isLocked()) {
                spinWait2.spinOne();
            }

            auto task2 = coroutine(false, false);
            while (mutex.isWaitListEmpty()) {
                spinWait2.spinOne();
            }
            task2.cancel();
            event2.signal();

            co_await task1;
            REQUIRE(value1 == 2);

            REQUIRE_THROWS_AS(co_await task2, CancellationError);
            REQUIRE(value2 == 0);
        };
        runner().get();
    }

    SECTION("after acquired")
    {
        auto runner = [&]() -> Sync<> {
            event1.signal();

            auto task1 = coroutine(true, false);
            AsyncSpinWait spinWait;
            while (!mutex.isLocked()) {
                spinWait.spinOne();
            }

            auto task2 = coroutine(false, false);
            while (mutex.isWaitListEmpty()) {
                spinWait.spinOne();
            }

            task1.cancel();
            event2.signal();

            co_await task2;
            REQUIRE(value2 == 2);

            REQUIRE_THROWS_AS(co_await task1, CancellationError);
            REQUIRE(value1 == 1);
        };
        runner().get();
    }

    SECTION("before acquire")
    {
        auto runner = [&]() -> Sync<> {
            auto task1 = coroutine(true, true);
            taskPtr = &task1;
            event1.signal();

            auto task2 = coroutine(false, false);
            event2.signal();

            co_await task2;
            REQUIRE(value2 == 2);

            REQUIRE_THROWS_AS(co_await task1, CancellationError);
            REQUIRE(value1 == 0);
        };
        runner().get();
    }
    REPEAT_FOOTER
}

TEST_CASE("Cancel - SharedTask", "[SharedTask]")
{
    REPEAT_HEADER
    AsyncEvent event;
    std::atomic<int> value = 0;
    std::atomic<bool> shouldThrow = false;
    std::atomic<Task<int>*> taskPtr;
    AsyncEvent taskPtrEvent;
    auto taskFunc = [&]() -> Task<int> {
        value++;
        co_await event;
        if (shouldThrow)
            throw std::runtime_error { "error" };
        co_return ++value;
    };

    auto sharedFunc = [&taskFunc, &taskPtr, &taskPtrEvent]() -> Async<int> {
        auto task = taskFunc();
        taskPtr = &task;
        taskPtrEvent.signal();
        auto v = co_await task;
        co_return ++v;
    };

    auto asyncFunc = [](Async<int>& shared) -> Async<int> {
        auto v = co_await shared;
        co_return ++v;
    };

    SECTION("no error or cancellation")
    {
        auto runner = [&]() -> SyncTask<> {
            auto shared = sharedFunc();
            auto async1 = asyncFunc(shared);
            auto async2 = asyncFunc(shared);
            event.signal();
            REQUIRE(co_await async1 == 4);
            REQUIRE(co_await async2 == 4);
        };
        runner().get();
    }

    SECTION("runtime error")
    {
        auto runner = [&]() -> SyncTask<> {
            auto shared = sharedFunc();
            auto async1 = asyncFunc(shared);
            auto async2 = asyncFunc(shared);
            shouldThrow = true;
            event.signal();
            REQUIRE_THROWS_AS(co_await async1, std::runtime_error);
            REQUIRE_THROWS_AS(co_await async2, std::runtime_error);
        };
        runner().get();
    }

    SECTION("cancel task")
    {
        auto runner = [&]() -> SyncTask<> {
            auto shared = sharedFunc();
            auto async1 = asyncFunc(shared);
            auto async2 = asyncFunc(shared);
            co_await taskPtrEvent;
            taskPtr.load()->cancel();
            event.signal();
            REQUIRE_THROWS_AS(co_await async1, CancellationError);
            REQUIRE_THROWS_AS(co_await async2, CancellationError);
        };
        runner().get();
    }

    SECTION("cancel shared")
    {
        auto runner = [&]() -> SyncTask<> {
            auto shared = sharedFunc();
            auto async1 = asyncFunc(shared);
            auto async2 = asyncFunc(shared);
            shared.cancel();
            event.signal();
            REQUIRE_THROWS_AS(co_await async1, CancellationError);
            REQUIRE_THROWS_AS(co_await async2, CancellationError);
        };
        runner().get();
    }
    REPEAT_FOOTER
}

TEST_CASE("Cancel - already cancelled task await event", "[AsyncTask][AsyncEvent]")
{
    REPEAT_HEADER
    AsyncEvent event1;
    AsyncEvent event2;
    std::atomic<int> value = 0;
    std::atomic<Async<>*> taskPtr;
    auto asyncFunc = [&]() -> Async<> {
        co_await event1;
        taskPtr.load()->cancel();
        value++;
        co_await event2;
        value++;
    };

    auto runner = [&]() -> SyncTask<> {
        auto task = asyncFunc();
        taskPtr = &task;
        event1.signal();
        REQUIRE_THROWS_AS(co_await task, CancellationError);
        REQUIRE(value == 1);
    };
    runner().get();
    REPEAT_FOOTER
}

TEST_CASE("Cancel - already cancelled task await mutex", "[AsyncTask][AsyncMutex]")
{
    REPEAT_HEADER
    AsyncMutex mutex;
    AsyncEvent event1;
    AsyncEvent event2;
    std::atomic<Async<>*> taskPtr;

    auto asyncFunc = [&]() -> Async<> {
        co_await event1;
        taskPtr.load()->cancel();
        auto lock = co_await mutex;
        (void)lock;
        co_await event2;
    };

    auto runner = [&]() -> SyncTask<> {
        auto task = asyncFunc();
        taskPtr = &task;
        event1.signal();
        REQUIRE_THROWS_AS(co_await task, CancellationError);
    };
    runner().get();
    REPEAT_FOOTER
}

TEST_CASE("Cancel - at barrier", "[AsyncBarrier]")
{
    REPEAT_HEADER
    AsyncBarrier barrier { 2 };
    std::atomic<int> value = 0;
    std::atomic<Async<>*> taskPtr;
    AsyncEvent event;

    auto asyncFunc = [&](bool cancel) -> Async<> {
        co_await event;
        if (cancel) {
            taskPtr.load()->cancel();
        }
        value++;
        co_await barrier;
        value++;
    };

    SECTION("no cancellation")
    {
        auto runner = [&]() -> SyncTask<> {
            event.signal();
            auto task1 = asyncFunc(false);
            taskPtr = &task1;
            auto task2 = asyncFunc(false);
            co_await task1;
            co_await task2;
            REQUIRE(value == 4);
            REQUIRE(barrier.isWaitListEmpty());
        };
        runner().get();
    }

    SECTION("cancel at barrier")
    {
        auto runner = [&]() -> SyncTask<> {
            auto task1 = asyncFunc(false);
            taskPtr = &task1;
            event.signal();
            AsyncSpinWait spinWait;
            while (barrier.isWaitListEmpty()) {
                spinWait.spinOne();
            }
            task1.cancel();
            auto task2 = asyncFunc(false);
            REQUIRE_THROWS_AS(co_await task2, CancellationError);
            REQUIRE_THROWS_AS(co_await task1, CancellationError);
            REQUIRE(value == 2);
            REQUIRE(barrier.isCanceled());
            REQUIRE(barrier.isWaitListEmpty());
        };
        runner().get();
    }

    SECTION("cancel before barrier")
    {
        SyncManualResetEvent completedEvent;
        auto runner = [&]() -> SyncTask<> {
            auto task1 = asyncFunc(true);
            taskPtr = &task1;
            event.signal();
            auto task2 = asyncFunc(false);
            REQUIRE_THROWS_AS(co_await task1, CancellationError);
            REQUIRE(value == 2);
            REQUIRE(!task2.isDone());
            REQUIRE(task2.status() == CoroutineStatus::Suspended);
            REQUIRE(!barrier.isWaitListEmpty());
            completedEvent.set();
        };
        auto runnerTask = runner();
        completedEvent.wait();
        barrier.cancel();
        runnerTask.get();
    }
    REPEAT_FOOTER
}

TEST_CASE("Cancel - generator", "[Generator]")
{
    auto generatorFunc = [&](bool throwException) -> Generator<int> {
        co_yield 1;
        if (throwException)
            throw std::runtime_error { "error" };
        co_yield 2;
        co_return 3;
    };

    SECTION("no cancellation")
    {
        auto runner = [&]() -> SyncTask<> {
            auto gen = generatorFunc(false);
            auto val1 = co_await gen;
            REQUIRE(val1 == 1);
            auto val2 = co_await gen;
            REQUIRE(val2 == 2);
            auto val3 = co_await gen;
            REQUIRE(val3 == 3);
            REQUIRE(gen.isDone());
        };
        runner().get();
    }

    SECTION("exception")
    {
        auto runner = [&]() -> SyncTask<> {
            auto gen = generatorFunc(true);
            auto val1 = co_await gen;
            REQUIRE(val1 == 1);
            bool isError = false;
            try {
                co_await gen;
            } catch (std::runtime_error&) {
                isError = true;
            }
            REQUIRE(isError);
            REQUIRE(gen.isDone());
        };
        runner().get();
    }

    SECTION("cancel")
    {
        auto runner = [&]() -> SyncTask<> {
            auto gen = generatorFunc(false);
            auto val1 = co_await gen;
            REQUIRE(val1 == 1);
            gen.cancel();
            bool isError = false;
            try {
                co_await gen;
            } catch (CancellationError&) {
                isError = true;
            }
            REQUIRE(isError);
            REQUIRE(gen.isDone());
        };
        runner().get();
    }
}

TEST_CASE("Cancel - destroy AsyncValue with awaiter", "[AsyncValue]")
{
    REPEAT_HEADER
    std::atomic<int> value = 0;
    auto coroutine = [&](auto* asyncValuePtr) -> Async<> {
        value++;
        co_await *asyncValuePtr;
        value++;
    };

    auto runner = [&]() -> Sync<> {
        auto asyncValuePtr = std::make_unique<AsyncValue<int>>();
        auto task = coroutine(asyncValuePtr.get());

        AsyncSpinWait spinWait;
        while (value == 0)
            spinWait.spinOne();

        while (asyncValuePtr->isWaitListEmpty())
            spinWait.spinOne();

        asyncValuePtr.reset();

        REQUIRE_THROWS_AS(co_await task, CancellationError);
        REQUIRE(value == 1);
        co_return;
    };
    runner().get();
    REPEAT_FOOTER
}

TEST_CASE("Cancel - destroy SharedAsyncValue with awaiters", "[SharedAsyncValue]")
{
    REPEAT_HEADER
    std::atomic<int> value = 0;
    auto coroutine = [&](auto* asyncValuePtr) -> Async<> {
        value++;
        co_await *asyncValuePtr;
        value++;
    };

    auto runner = [&]() -> Sync<> {
        auto asyncValuePtr = std::make_unique<AsyncValue<int>>();
        auto task = coroutine(asyncValuePtr.get());

        AsyncSpinWait spinWait;
        while (value == 0)
            spinWait.spinOne();

        while (asyncValuePtr->isWaitListEmpty())
            spinWait.spinOne();

        asyncValuePtr.reset();

        REQUIRE_THROWS_AS(co_await task, CancellationError);
        REQUIRE(value == 1);
    };
    runner().get();
    REPEAT_FOOTER
}

TEST_CASE("Cancel - destroy AsyncBarrier with awaiters", "[AsyncBarrier]")
{
    REPEAT_HEADER
    std::atomic<int> value = 0;
    auto coroutine = [&](AsyncBarrier* barrierPtr) -> Async<> {
        value++;
        co_await *barrierPtr;
        value++;
    };

    auto runner = [&]() -> Sync<> {
        auto barrierPtr = std::make_unique<AsyncBarrier>(2);
        auto task = coroutine(barrierPtr.get());

        AsyncSpinWait spinWait;
        while (value == 0)
            spinWait.spinOne();

        while (barrierPtr->isWaitListEmpty())
            spinWait.spinOne();

        barrierPtr.reset();

        REQUIRE_THROWS_AS(co_await task, CancellationError);
        REQUIRE(value == 1);
    };
    runner().get();
    REPEAT_FOOTER
}

namespace {
    template <template <typename> typename TaskType>
    auto leakTestFunc = [](Counted counted) -> TaskType<Counted> {
        AsyncTestUtils::randomSpinWait(100);
        co_return counted;
    };

    template <template <typename> typename TaskType>
    auto runner = [](bool abandon, bool cancel = false) -> Sync<> {
        auto task = leakTestFunc<TaskType>({});
        if (abandon) {
            AsyncTestUtils::randomSpinWait(100);
            if (cancel)
                task.cancel();
        } else {
            co_await task;
        }
        co_return;
    };

    auto checkLeaks = []() {
        SpinWait spinWait;
        while (Counted::activeCount() != 0)
            spinWait.spinOne();
    };
}

TEST_CASE("Cancel - destruction & leak checks", "[AsyncTask]")
{
    SECTION("async leak check")
    {
        REPEAT_HEADER
        runner<Async>(false).get();
        REPEAT_FOOTER
        checkLeaks();
    }

    SECTION("async leak check with abandoned tasks")
    {
        REPEAT_HEADER
        runner<Async>(true).get();
        REPEAT_FOOTER
        checkLeaks();
    }

    SECTION("async leak check with abandoned & cancelled tasks")
    {
        REPEAT_HEADER
        runner<Async>(true, true).get();
        REPEAT_FOOTER
        checkLeaks();
    }

    SECTION("shared leak check")
    {
        REPEAT_HEADER
        runner<Async>(false).get();
        REPEAT_FOOTER
        checkLeaks();
    }

    SECTION("shared leak check with abandoned tasks")
    {
        REPEAT_HEADER
        runner<Async>(true).get();
        REPEAT_FOOTER
        checkLeaks();
    }

    SECTION("shared leak check with abandoned & cancelled tasks")
    {
        REPEAT_HEADER
        runner<Async>(true, true).get();
        REPEAT_FOOTER
        checkLeaks();
    }

    SECTION("task leak check")
    {
        REPEAT_HEADER
        runner<DefaultTask>(false).get();
        REPEAT_FOOTER
        checkLeaks();
    }

    SECTION("Immediately cancel async")
    {
        auto runner = [&]() -> Sync<> {
            leakTestFunc<Async>({}).cancel();
            AsyncTestUtils::randomSpinWait(100);

            co_return;
        };

        REPEAT_HEADER
        runner().get();
        REPEAT_FOOTER
        checkLeaks();
    }

    SECTION("Immediately abandon async")
    {
        auto runner = [&]() -> Sync<> {
            (void)leakTestFunc<Async>({});
            AsyncTestUtils::randomSpinWait(100);

            co_return;
        };

        REPEAT_HEADER
        runner().get();
        REPEAT_FOOTER
        checkLeaks();
    }

    auto scheduledFunc = [&]() -> BackgroundAsync<> {
        AsyncTestUtils::randomSpinWait(100);
        co_return;
    };

    auto scheduledFuncRunner = [&](bool cancel) -> Sync<> {
        auto task = scheduledFunc();
        AsyncTestUtils::randomSpinWait(100);
        if (cancel)
            task.cancel();
        co_return;
    };

    SECTION("abandon scheduled async")
    {
        REPEAT_HEADER
        scheduledFuncRunner(false).get();
        REPEAT_FOOTER
        checkLeaks();
    }

    SECTION("cancel scheduled async")
    {
        REPEAT_HEADER
        scheduledFuncRunner(true).get();
        REPEAT_FOOTER
        checkLeaks();
    }
}

TEST_CASE("Cancel - yield leak checks", "[Task]")
{
    std::default_random_engine rng { std::random_device {}() };

    auto leakTestFunc = [&](Counted counted) -> Task<Counted> {
        AsyncTestUtils::randomSpinWait(100);
        co_yield counted;
        co_return counted;
    };

    auto cancellerFunc = [&](Task<Counted>& task) -> Async<> {
        AsyncTestUtils::randomSpinWait(100);
        task.cancel();
        co_return;
    };

    auto runner = [&](bool cancel, auto& rng) -> Sync<> {
        auto task = leakTestFunc({});

        Async<> canceller;
        if (cancel)
            canceller = cancellerFunc(task);
        try {
            (void)co_await task;
        } catch (CancellationError&) {
            REQUIRE(task.isCancelled());
        }
        if (cancel)
            co_await canceller;

        co_return;
    };

    SECTION("yield leak check")
    {
        runner(false, rng).get();
        checkLeaks();
    }

    SECTION("yield leak check with cancelled tasks")
    {
        REPEAT_HEADER
        runner(true, rng).get();
        REPEAT_FOOTER
        checkLeaks();
    }
}

TEST_CASE("Cancel - cancelled task await on AsyncValue", "[AsyncValue]")
{
    REPEAT_HEADER
    std::atomic<int> value = 0;
    AsyncValue<void> sharedAsyncValue;
    auto coroutine = [&]() -> Async<> {
        value++;
        AsyncTestUtils::randomSpinWait(100);
        co_await sharedAsyncValue;
        value++;
    };

    auto cancellerFunc = [&](Async<>& task) -> Async<> {
        AsyncTestUtils::randomSpinWait(100);
        task.cancel();
        co_return;
    };

    auto runner = [&]() -> Sync<> {
        auto task = coroutine();
        auto canceller = cancellerFunc(task);
        AsyncSpinWait spinWait;
        while (value == 0 && !task.isCancelled())
            spinWait.spinOne();

        sharedAsyncValue.setAndSignal();
        co_await canceller;

        bool isCancellationError = false;
        try {
            co_await task;
        } catch (CancellationError&) {
            isCancellationError = true;
        }
        REQUIRE((isCancellationError || value == 2));
    };
    runner().get();
    REPEAT_FOOTER
}

TEST_CASE("Task - immediately cancel async blocked on task", "[Task]")
{
    REPEAT_HEADER
    std::atomic<int> value1 = 0;
    std::atomic<int> value2 = 0;
    AsyncEvent event;

    auto blockOnEvent = [&]() -> Task<> {
        value1 = 1;
        co_await event;
        value1 = 2;
    };

    auto blockOnTask = [&]() -> Async<> {
        auto blockOnEventTask = blockOnEvent();
        value2 = 1;
        co_await blockOnEventTask;
        value2 = 2;
    };

    auto runner = [&]() -> Sync<> {
        auto blockOnTaskAsync = blockOnTask();

        AsyncSpinWait asyncSpinWait;
        while (event.isWaitListEmpty())
            asyncSpinWait.spinOne();

        blockOnTaskAsync.cancel();

        REQUIRE_THROWS_AS(co_await blockOnTaskAsync, CancellationError);

        while (!event.isWaitListEmpty())
            asyncSpinWait.spinOne();
        co_return;
    };

    runner().get();

    REQUIRE(value1 == 1);
    REQUIRE(value2 == 1);

    REPEAT_FOOTER
}

}
