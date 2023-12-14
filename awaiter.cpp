//
// Created by irantha on 6/11/23.
//

#include "awaiter.hpp"
#include "event/async_barrier.hpp"
#include "event/async_mutex.hpp"
#include "event/async_value.hpp"
#include "task/cancellation_error.hpp"
#include "async_spin_wait.hpp"
#include "coroutine.hpp"
#include "task/task_awaiter.hpp"
#include "thread_pool.hpp"

namespace Levelz::Async {

Awaiter::Awaiter(Coroutine& coroutine, AwaiterKind kind) noexcept
    : m_coroutine { coroutine }
    , m_kind { kind }
    , m_maybeBlocked { false }
{
}

Coroutine& Awaiter::coroutine() const noexcept
{
    return m_coroutine;
}

Awaiter::SuspensionAdvice Awaiter::onSuspend(std::coroutine_handle<> handle)
{
    auto isFinalAwaiter = kind() == AwaiterKind::Final;
    if (ThreadPool::currentCoroutine() == &m_coroutine)
        ThreadPool::setCurrentCoroutine(nullptr);

    assert(handle == m_coroutine.m_handle);
    auto isCancelled = m_coroutine.isCancelled();
    auto shouldCancelAbandoned = m_coroutine.shouldCancelAbandoned();
    auto threadPoolKind = m_coroutine.threadPoolKind();

    auto status = m_coroutine.setStatus(CoroutineStatus::Suspended, isFinalAwaiter);

    if (status == CoroutineStatus::AbandonedCompleted) {
        assert(isFinalAwaiter);
        return SuspensionAdvice::shouldNotSuspend;
    }

    if (!isFinalAwaiter && threadPoolKind != ThreadPool::currentThreadPoolKind())
        return SuspensionAdvice::shouldSuspend;

    if (!ThreadPool::canDoChainedExecution())
        return SuspensionAdvice::shouldSuspend;

    if (!isFinalAwaiter && status == CoroutineStatus::Abandoned && shouldCancelAbandoned)
        return SuspensionAdvice::shouldNotSuspend;

    if (!isFinalAwaiter && isCancelled)
        return SuspensionAdvice::shouldNotSuspend;

    return SuspensionAdvice::maySuspend;
}

void Awaiter::onResume()
{
    assert(coroutine().threadPoolKind() == ThreadPool::currentThreadPoolKind());

    if (kind() != AwaiterKind::Final && kind() != AwaiterKind::Initial && kind() != AwaiterKind::Yield)
        coroutine().removeAwaiter(this);

    ThreadPool::setCurrentCoroutine(&coroutine());

    if (ThreadPool::isImmediateShutdownRequested()) {
        m_coroutine.cancel();
        throw CancellationError::shutdownCancellationError();
    }

    auto prevStatus = m_coroutine.setStatus(CoroutineStatus::Running);
    if (prevStatus == CoroutineStatus::PauseOnRunning) {
        AsyncSpinWait asyncSpinWait;
        while (m_coroutine.status() == CoroutineStatus::Paused)
            asyncSpinWait.spinOne();
    }

    ThreadPool::recordChainedExecution();

    if ((m_coroutine.status() == CoroutineStatus::Abandoned && m_coroutine.shouldCancelAbandoned())
        || m_coroutine.isCancelled())
        throw CancellationError {};
}

Awaiter::SuspensionAdvice Awaiter::onReady()
{
    auto isFinalAwaiter = kind() == AwaiterKind::Final;
    if (isFinalAwaiter)
        ThreadPool::setCurrentCoroutine(nullptr);

    if (kind() != AwaiterKind::Final && kind() != AwaiterKind::Initial && kind() != AwaiterKind::Yield)
        coroutine().addAwaiter(this);

    if (m_coroutine.status() == CoroutineStatus::AbandonedCompleted) {
        assert(isFinalAwaiter);
        return SuspensionAdvice::shouldNotSuspend;
    }

    if (coroutine().threadPoolKind() != ThreadPool::currentThreadPoolKind())
        return SuspensionAdvice::shouldSuspend;

    if (!ThreadPool::canDoChainedExecution())
        return SuspensionAdvice::shouldSuspend;

    if (!isFinalAwaiter && m_coroutine.status() == CoroutineStatus::Abandoned && m_coroutine.shouldCancelAbandoned())
        return SuspensionAdvice::shouldNotSuspend;

    if (!isFinalAwaiter && m_coroutine.isCancelled())
        return SuspensionAdvice::shouldNotSuspend;

    return SuspensionAdvice::maySuspend;
}

Awaiter* Awaiter::next() const noexcept
{
    return m_next;
}

void Awaiter::setNext(Awaiter* awaiter) noexcept
{
    m_next = awaiter;
}

AwaiterKind Awaiter::kind() const noexcept
{
    return m_kind;
}

bool Awaiter::cancel(Awaiter* awaiter) noexcept
{
    switch (awaiter->kind()) {
    case AwaiterKind::Event:
        return static_cast<AsyncCountDownEvent::AsyncCountDownEventAwaiter*>(awaiter)->cancel();
    case AwaiterKind::Value:
        return static_cast<BaseAsyncValueAwaiter*>(awaiter)->cancel();
    case AwaiterKind::Mutex:
        return static_cast<AsyncMutexAwaiter*>(awaiter)->cancel();
    case AwaiterKind::Barrier:
        return static_cast<AsyncBarrier::AsyncBarrierAwaiter*>(awaiter)->cancel();
    case AwaiterKind::Task:
        return static_cast<TaskAwaiterBase*>(awaiter)->cancel();
    default:
        break;
    }
    return false;
}

bool Awaiter::maybeBlocked() const noexcept
{
    return m_maybeBlocked;
}

void Awaiter::setMaybeBlocked(bool maybeBlocked) noexcept
{
    m_maybeBlocked = maybeBlocked;
}

}
