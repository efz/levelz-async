# Lock free C++ Coroutine Library #

Inspired by https://github.com/lewissbaker/cppcoro

## Coroutines ##

1. Task
2. AsyncTask
3. SyncTask

### 1. Task ###
Starts only when is _co_awaited_ by another coroutine.
Support returning value by copy, reference and by move.

### 2. AsyncTask ###
Similar to _Task_ but starts immediately upon creation.

### 2. SyncTask ###
Starts immediately upon creation like _AsyncTask_. 
Has an embedded _conditional variable_ so main thread can 
synchronously wait for it result.
Only returns after all it's child _Tasks_ have completed execution 
(transitively), which guarantees no user code execution after
_SyncTask_ return.

## Primitives ##

### AsyncMutex ###
Lock free none-blocking mutex.

```c++
AsyncMutx mutex;
int value = 0;

auto incr = [&]() -> Async<> {
    for (int i = 0; i++; i < 1000) {
        auto lock = co_await mutex;
        value++;
    }
};

auto decr = [&]() -> Async<> {
    for (int i = 0; i++; i < 1000) {
        auto lock = co_await mutex;
        value--;
    }
};

auto runner = [&]() -> Sync<> {
    auto incrTask = incr();
    auto decrTask = decr();
    co_await incrTask;
    co_await decrTask;
    REQUIRE(value == 0);
};
runner().get();
```

### Event ###
```c++
AsyncEvent event;
int value = 0;

auto producer = [&]() -> Async<> {
    value++;
    event.set();
};

auto runner = [&]() -> Sync<> {
    auto producerTask = producer();
    co_await event;
    REQUIRE(value == 1);
};
runner().get();

```

_AsyncCountDownEvent_ support, _countUp(), CountDown() 
and isZero()_ methods instead of _set(), reset() and isSet()_ methods on binary event.

### Value ###
Similar to AsyncEvent but support returning a value.

```c++
AsyncValue<int> value;

auto producer = [&]() -> Async<> {
    value.setAndSignal(10);
};

auto runner = [&]() -> Sync<> {
    auto producerTask = producer();
    auto result = co_await value;
    REQUIRE(result == 10);
};
runner().get();

```

### Barrier ###

```c++
AsyncBarrier barrier { 2 };
int value1;
int value2;

auto transformer1 = [&]() -> Async<> {
    value1 = 10;
    co_await barrier;
    value1 = value1 + value2 + 2;
};

auto transformer1 = [&]() -> Async<> {
    value2 = 20;
    co_await barrier;
    value2 = value1 + value2 + 3;
};

auto runner = [&]() -> Sync<> {
    auto transformer1Task = transformer1();
    auto transformer2Task = transformer2();
    co_await transformer1Task;
    co_await transformer2Task;
    REQUIRE(value1 == 32);
    REQUIRE(value2 == 33;
};
runner().get();

```

