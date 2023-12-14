//
// Created by irantha on 12/13/23.
//

#ifndef LEVELZ_ASYNC_COUNTDOWN_EVENT_HPP
#define LEVELZ_ASYNC_COUNTDOWN_EVENT_HPP

#include <atomic>
#include <cassert>
#include <coroutine>
#include <limits>

#include "awaiter.hpp"
#include "fifo_wait_list.hpp"
#include "thread_pool_kind.hpp"
#include "async_scope.hpp"
#include "sync_manual_reset_event.hpp"

namespace Levelz::Async {

struct AsyncCountDownEvent {

    AsyncCountDownEvent(bool initiallyZero = false, int maxCount = std::numeric_limits<int>::max()) noexcept;
    ~AsyncCountDownEvent();
    bool isWaitListEmpty() const noexcept;
    uint64_t waitListedCount() const noexcept;
    bool isZero() const noexcept;
    bool countUp() noexcept;
    bool countDown() noexcept;
    void enqueue(AsyncCountDownEvent& event) noexcept;
    void enqueue(SyncManualResetEvent& event) noexcept;
    uint64_t count() const noexcept;

    struct AsyncCountDownEventAwaiter : Awaiter {
    public:
        AsyncCountDownEventAwaiter(AsyncCountDownEvent& event, Coroutine& coroutine) noexcept;

        AsyncCountDownEventAwaiter(const AsyncCountDownEventAwaiter&) = delete;
        AsyncCountDownEventAwaiter& operator=(const AsyncCountDownEventAwaiter&) = delete;
        AsyncCountDownEventAwaiter(AsyncCountDownEventAwaiter&&) = delete;
        AsyncCountDownEventAwaiter& operator=(AsyncCountDownEventAwaiter&&) = delete;

        bool await_ready() noexcept;
        bool await_suspend(std::coroutine_handle<> awaitingCoroutineHandle) noexcept;
        void await_resume();
        bool cancel() noexcept;

    private:
        AsyncCountDownEvent& m_event;
    };

    using AwaiterType = AsyncCountDownEventAwaiter;

private:
    friend struct BasePromise;
    template <typename>
    friend struct BaseAsyncValue;
    template <typename, ThreadPoolKind>
    friend struct SyncTaskPromiseBase;
    friend struct BaseAsyncValueAwaiter;
    friend struct AsyncEvent;

    bool remove(Coroutine* coroutineToRemove) noexcept;
    void resumeWaiting() noexcept;
    bool enqueue(Coroutine& coroutine) noexcept;
    void enqueueAlways(Coroutine& simpleTaskCoroutine) noexcept;

    mutable FifoWaitList m_waitQueue;
    mutable std::atomic<int> m_count;
    mutable AsyncScope m_asyncScope;
    const int m_maxCount;
};

}

#endif // LEVELZ_ASYNC_COUNTDOWN_EVENT_HPP
