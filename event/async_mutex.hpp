//
// Created by irantha on 6/4/23.
//

#ifndef LEVELZ_ASYNC_MUTEX_HPP
#define LEVELZ_ASYNC_MUTEX_HPP

#include <coroutine>

#include "awaiter.hpp"
#include "coroutine.hpp"
#include "fifo_wait_list.hpp"
#include "task/cancellation_error.hpp"
#include "thread_pool.hpp"
#include "async_scope.hpp"

namespace Levelz::Async {

struct AsyncMutexAwaiter;

struct AsyncMutex {
    using AwaiterType = AsyncMutexAwaiter;

    AsyncMutex() noexcept
        : m_waitList {}
        , m_ownerCoroutine { nullptr }
        , m_asyncScope {}
        , m_cancelled { false }
    {
    }

    ~AsyncMutex();

    AsyncMutex(const AsyncMutex&) = delete;
    AsyncMutex& operator=(const AsyncMutex&) = delete;
    AsyncMutex(AsyncMutex&&) = delete;
    AsyncMutex& operator=(AsyncMutex&&) = delete;

    bool isLocked() const noexcept;
    bool isWaitListEmpty() const noexcept;
    void cancel() noexcept;
    bool isCancelled() const noexcept;
    uint64_t waitListedCount() const noexcept;

private:
    friend struct AsyncMutexAwaiter;
    friend struct AsyncScopedUnlock;

    bool tryLock(Coroutine* awaiterCoroutine);
    void lock(Coroutine* awaiterCoroutine);
    void unlock(Coroutine* awaiterCoroutine);
    bool remove(Coroutine* coroutineToRemove) noexcept;

    FifoWaitList m_waitList;
    std::atomic<Coroutine*> m_ownerCoroutine;
    AsyncScope m_asyncScope;
    std::atomic<bool> m_cancelled;
};

struct [[nodiscard]] AsyncScopedUnlock {
    AsyncScopedUnlock(const AsyncScopedUnlock&) = delete;
    AsyncScopedUnlock& operator=(const AsyncScopedUnlock&) = delete;

    AsyncScopedUnlock(AsyncScopedUnlock&& other) noexcept
        : m_coroutine { other.m_coroutine }
        , m_mutex { other.m_mutex }
    {
        other.m_mutex = nullptr;
    }

    ~AsyncScopedUnlock()
    {
        if (m_mutex)
            m_mutex->unlock(&m_coroutine);
    }

private:
    friend struct AsyncMutexAwaiter;

    AsyncScopedUnlock(AsyncMutex& mutex, Coroutine& coroutine) noexcept
        : m_mutex { &mutex }
        , m_coroutine { coroutine }
    {
        assert(m_coroutine.validate());
    }

    AsyncMutex* m_mutex;
    Coroutine& m_coroutine;
};

struct AsyncMutexAwaiter : Awaiter {
    AsyncMutexAwaiter(AsyncMutex& mutex, Coroutine& coroutine) noexcept
        : Awaiter { coroutine, AwaiterKind::Mutex }
        , m_mutex { mutex }
    {
        assert(coroutine.validate());
    }

    bool await_ready() noexcept
    {
        auto suspensionAdvice = Awaiter::onReady();
        if (suspensionAdvice == Awaiter::SuspensionAdvice::shouldNotSuspend)
            return true;
        if (suspensionAdvice == Awaiter::SuspensionAdvice::shouldSuspend)
            return false;

        if (m_mutex.isCancelled())
            return true;

        return m_mutex.tryLock(&coroutine());
    }

    bool await_suspend(std::coroutine_handle<> awaitingCoroutineHandle) noexcept
    {
        Awaiter::SuspensionAdvice suspensionAdvice = Awaiter::onSuspend(awaitingCoroutineHandle);
        if (suspensionAdvice == Awaiter::SuspensionAdvice::shouldNotSuspend)
            return false;
        bool mutexIsCancelled = m_mutex.isCancelled();

        if (mutexIsCancelled) {
            coroutine().setCancelled();
            if (suspensionAdvice == Awaiter::SuspensionAdvice::shouldSuspend) {
                coroutine().schedule();
                return true;
            } else {
                return false;
            }
        }

        setMaybeBlocked(true);
        m_mutex.lock(&coroutine());
        return true;
    }

    [[nodiscard]] AsyncScopedUnlock await_resume()
    {
        if (!coroutine().isCancelled() && m_mutex.isCancelled())
            coroutine().setCancelled();

        auto lock = AsyncScopedUnlock { m_mutex, coroutine() };
        Awaiter::onResume();
        return lock;
    }

    bool cancel() noexcept
    {
        auto& coroutine = Awaiter::coroutine();
        assert(coroutine.isCancelled());
        assert(coroutine.status() == CoroutineStatus::PauseOnRunning || coroutine.status() == CoroutineStatus::Paused);
        auto removed = m_mutex.remove(&coroutine);
        if (removed)
            coroutine.schedule();
        return removed;
    }

private:
    AsyncMutex& m_mutex;
};

}

#endif // LEVELZ_ASYNC_MUTEX_HPP
