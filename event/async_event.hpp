//
// Created by irantha on 5/13/23.
//

#ifndef LEVELZ_ASYNC_EVENT_HPP
#define LEVELZ_ASYNC_EVENT_HPP

#include <atomic>
#include <cassert>
#include <coroutine>

#include "awaiter.hpp"
#include "fifo_wait_list.hpp"
#include "thread_pool_kind.hpp"
#include "async_countdown_event.hpp"
#include "async_scope.hpp"
#include "sync_manual_reset_event.hpp"

namespace Levelz::Async {

struct Coroutine;

struct AsyncEvent {
    explicit AsyncEvent(bool initiallySet = false) noexcept;
    void signal() noexcept;
    void reset() noexcept;
    bool isWaitListEmpty() const noexcept;
    uint64_t waitListedCount() const noexcept;
    bool isSignaled() const noexcept;
    void enqueue(AsyncEvent& event) noexcept;
    void enqueue(SyncManualResetEvent& event) noexcept;

    using AwaiterType = AsyncCountDownEvent::AsyncCountDownEventAwaiter;

private:
    friend struct BasePromise;
    template <typename>
    friend struct BaseAsyncValue;
    template <typename, ThreadPoolKind>
    friend struct SyncTaskPromiseBase;
    friend struct BaseAsyncValueAwaiter;

    bool enqueue(Coroutine& coroutine) noexcept;
    bool remove(Coroutine* coroutineToRemove) noexcept;

    AsyncCountDownEvent m_countDownEvent;
};

}

#endif // LEVELZ_ASYNC_EVENT_HPP
