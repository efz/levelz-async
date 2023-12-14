//
// Created by irantha on 6/8/23.
//

#ifndef LEVELZ_ASYNC_BARRIER_HPP
#define LEVELZ_ASYNC_BARRIER_HPP

#include <atomic>
#include <coroutine>

#include "awaiter.hpp"
#include "coroutine.hpp"
#include "fifo_wait_list.hpp"
#include "task/cancellation_error.hpp"
#include "thread_pool.hpp"
#include "async_scope.hpp"

namespace Levelz::Async {

struct AsyncBarrier {
    explicit AsyncBarrier(int capacity) noexcept
        : m_capacity { capacity }
        , m_count { 0 }
        , m_waitList {}
        , m_asyncScope {}
        , m_canceled { false }
    {
    }

    ~AsyncBarrier();
    [[nodiscard]] bool isWaitListEmpty() const noexcept;
    void cancel() noexcept;
    bool isCanceled() const noexcept;
    void reset() noexcept;
    uint64_t waitListedCount() const noexcept;

    struct AsyncBarrierAwaiter : Awaiter {
        AsyncBarrierAwaiter(AsyncBarrier& barrier, Coroutine& coroutine) noexcept
            : Awaiter { coroutine, AwaiterKind::Barrier }
            , m_barrier { barrier }
        {
        }

        [[nodiscard]] bool await_ready() noexcept
        {
            auto suspensionAdvice = Awaiter::onReady();
            if (suspensionAdvice == Awaiter::SuspensionAdvice::shouldNotSuspend)
                return true;
            if (suspensionAdvice == Awaiter::SuspensionAdvice::shouldSuspend)
                return false;
            if (m_barrier.isCanceled())
                return true;
            return m_barrier.arriveAndRelease(nullptr);
        }

        bool await_suspend(std::coroutine_handle<> awaitingCoroutineHandle) noexcept
        {
            auto suspensionAdvice = Awaiter::onSuspend(awaitingCoroutineHandle);
            if (suspensionAdvice == Awaiter::SuspensionAdvice::shouldNotSuspend)
                return false;

            setMaybeBlocked(true);
            auto needToKeepSuspended = !m_barrier.isCanceled() && m_barrier.arriveAndWait(&coroutine());
            setMaybeBlocked(needToKeepSuspended);

            if (suspensionAdvice == Awaiter::SuspensionAdvice::shouldSuspend) {
                if (!needToKeepSuspended)
                    coroutine().schedule();
                return true;
            }

            return needToKeepSuspended;
        }

        void await_resume()
        {
            Awaiter::onResume();
            if (m_barrier.isCanceled())
                throw CancellationError {};
        }

        bool cancel() noexcept
        {
            auto& coroutine = Awaiter::coroutine();
            (void)coroutine;
            assert(coroutine.isCancelled());
            assert(coroutine.status() == CoroutineStatus::PauseOnRunning || coroutine.status() == CoroutineStatus::Paused);
            m_barrier.cancel();
            return true;
        }

    private:
        AsyncBarrier& m_barrier;
    };

    using AwaiterType = AsyncBarrierAwaiter;

private:
    bool arriveAndWait(Coroutine* awaiter) noexcept;
    bool arriveAndRelease(Coroutine* awaiter) noexcept;

    FifoWaitList m_waitList;
    std::atomic<int> m_count;
    const int m_capacity;
    AsyncScope m_asyncScope;
    std::atomic<bool> m_canceled;
};

}

#endif // LEVELZ_ASYNC_BARRIER_HPP
