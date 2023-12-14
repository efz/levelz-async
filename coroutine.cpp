//
// Created by irantha on 6/11/23.
//

#include <cassert>

#include "coroutine.hpp"
#include "thread_pool.hpp"

namespace Levelz::Async {

Coroutine::Coroutine(std::coroutine_handle<> handle, bool cancelAbandoned, TaskKind taskKind, ThreadPoolKind threadPoolKind) noexcept
    : m_next { nullptr }
    , m_handle { handle }
    , m_status { CoroutineStatus::NotStarted }
    , m_cancelled { false }
    , m_cancelAbandoned { cancelAbandoned }
    , m_owner { nullptr }
    , m_awaiters {}
    , m_threadPoolKind { determineThreadPoolKind(threadPoolKind) }
#ifdef DEBUG
    , m_taskKind { taskKind }
    , m_completionEvent {}
#endif
{
}

Coroutine::~Coroutine()
{
    assert(ThreadPool::currentCoroutine() != this);
}

void Coroutine::setNext(Coroutine* next) noexcept
{
    m_next.store(next);
}

Coroutine* Coroutine::next() const noexcept
{
    return m_next.load();
}

std::coroutine_handle<> Coroutine::handle() noexcept
{
    return m_handle;
}

void Coroutine::resume()
{
    assert(ThreadPool::canDoChainedExecution());
    assert(!m_handle.done());
    m_handle.resume();
}

void Coroutine::schedule() noexcept
{
    assert(!m_handle.done());
    ThreadPool::threadPool(threadPoolKind()).scheduleOnThreadPool(this);
}

bool Coroutine::validate() const noexcept
{
    return m_handle != nullptr;
}

CoroutineStatus Coroutine::status() const noexcept
{
    return m_status;
}

void Coroutine::justSetStatus(CoroutineStatus newStatus, CoroutineStatus expectedCurrentStatus) noexcept
{
    auto prevStatus = m_status.exchange(newStatus);
    if (prevStatus != expectedCurrentStatus)
        std::abort();
}

CoroutineStatus Coroutine::setStatus(CoroutineStatus status, bool isFinalAwaiter) noexcept
{
    auto currentStatus = m_status.load();
    CoroutineStatus newStatus;
    do {
        switch (currentStatus) {
        case CoroutineStatus::NotStarted: {
            switch (status) {
            case CoroutineStatus::PauseOnRunning:
            case CoroutineStatus::Suspended:
                return CoroutineStatus::NotStarted;
            case CoroutineStatus::Abandoned:
                newStatus = CoroutineStatus::Abandoned;
                break;
            case CoroutineStatus::Running:
                newStatus = CoroutineStatus::Running;
                break;
            default:
                std::abort();
            }
            break;
        }

        case CoroutineStatus::Returned: {
            switch (status) {
            case CoroutineStatus::PauseOnRunning:
                return CoroutineStatus::Returned;
            case CoroutineStatus::Abandoned:
                newStatus = CoroutineStatus::Abandoned;
                break;
            case CoroutineStatus::Completed:
                newStatus = CoroutineStatus::ReturnedCompleted;
                break;
            case CoroutineStatus::Suspended:
                assert(isFinalAwaiter);
                newStatus = CoroutineStatus::FinalSuspended;
                break;
            default:
                std::abort();
            }
            break;
        }

        case CoroutineStatus::Yielded: {
            switch (status) {
            case CoroutineStatus::PauseOnRunning:
                return CoroutineStatus::Yielded;
            case CoroutineStatus::Abandoned:
                newStatus = CoroutineStatus::Abandoned;
                break;
            case CoroutineStatus::Suspended:
                newStatus = CoroutineStatus::YieldedSuspended;
                break;
            case CoroutineStatus::Running:
                assert(m_cancelled.load());
                newStatus = CoroutineStatus::Running;
                break;
            default:
                std::abort();
            }
            break;
        }

        case CoroutineStatus::FinalSuspended: {
            switch (status) {
            case CoroutineStatus::PauseOnRunning:
                return CoroutineStatus::FinalSuspended;
            case CoroutineStatus::Abandoned:
                newStatus = CoroutineStatus::AbandonedFinalSuspended;
                break;
            case CoroutineStatus::Completed:
                newStatus = CoroutineStatus::CompletedFinalSuspended;
                break;
            default:
                std::abort();
            }
            break;
        }

        case CoroutineStatus::YieldedSuspended: {
            switch (status) {
            case CoroutineStatus::PauseOnRunning:
                return CoroutineStatus::YieldedSuspended;
            case CoroutineStatus::Abandoned:
                newStatus = CoroutineStatus::AbandonedYieldSuspended;
                break;
            case CoroutineStatus::Running:
                newStatus = CoroutineStatus::Running;
                break;
            default:
                std::abort();
            }
            break;
        }

        case CoroutineStatus::ReturnedCompleted: {
            switch (status) {
            case CoroutineStatus::PauseOnRunning:
                return CoroutineStatus::ReturnedCompleted;
            case CoroutineStatus::Abandoned:
                newStatus = CoroutineStatus::AbandonedCompleted;
                break;
            case CoroutineStatus::Suspended:
                assert(isFinalAwaiter);
                newStatus = CoroutineStatus::CompletedFinalSuspended;
                break;
            default:
                std::abort();
            }
            break;
        }

        case CoroutineStatus::Running: {
            switch (status) {
            case CoroutineStatus::Running:
            case CoroutineStatus::PauseOnRunning:
                return CoroutineStatus::Running;
            case CoroutineStatus::Suspended:
                newStatus = CoroutineStatus::Suspended;
                break;
            case CoroutineStatus::Returned:
                newStatus = CoroutineStatus::Returned;
                break;
            case CoroutineStatus::Yielded:
                newStatus = CoroutineStatus::Yielded;
                break;
            case CoroutineStatus::Abandoned:
                newStatus = CoroutineStatus::Abandoned;
                break;
            default:
                std::abort();
            }
            break;
        }

        case CoroutineStatus::Suspended: {
            switch (status) {
            case CoroutineStatus::Suspended:
                return CoroutineStatus::Suspended;
            case CoroutineStatus::Running:
                newStatus = CoroutineStatus::Running;
                break;
            case CoroutineStatus::Abandoned:
                newStatus = CoroutineStatus::Abandoned;
                break;
            case CoroutineStatus::PauseOnRunning:
                newStatus = CoroutineStatus::PauseOnRunning;
                break;
            default:
                std::abort();
            }
            break;
        }

        case CoroutineStatus::Completed:
        case CoroutineStatus::Resumed: {
            switch (status) {
            default:
                std::abort();
            }
        }

        case CoroutineStatus::Abandoned: {
            switch (status) {
            case CoroutineStatus::Suspended:
                newStatus = isFinalAwaiter
                    ? CoroutineStatus::AbandonedFinalSuspended
                    : CoroutineStatus::Abandoned;
                break;
            case CoroutineStatus::Completed:
                newStatus = CoroutineStatus::AbandonedCompleted;
                break;
            case CoroutineStatus::Running:
            case CoroutineStatus::Returned:
            case CoroutineStatus::Yielded:
                newStatus = CoroutineStatus::Abandoned;
                break;
            default:
                std::abort();
            }
            break;
        }

        case CoroutineStatus::AbandonedFinalSuspended: {
            switch (status) {
            case CoroutineStatus::Completed:
                return CoroutineStatus::AbandonedFinalSuspended;
            default:
                std::abort();
            }
        }

        case CoroutineStatus::AbandonedYieldSuspended: {
            switch (status) {
            case CoroutineStatus::Running:
                assert(isCancelled());
                newStatus = CoroutineStatus::Abandoned;
                break;
            default:
                std::abort();
            }
            break;
        }

        case CoroutineStatus::AbandonedCompleted: {
            switch (status) {
            case CoroutineStatus::Suspended:
                assert(isFinalAwaiter);
                return CoroutineStatus::AbandonedCompleted;
            default:
                std::abort();
            }
        }

        case CoroutineStatus::CompletedFinalSuspended: {
            switch (status) {
            case CoroutineStatus::Abandoned:
                return CoroutineStatus::CompletedFinalSuspended;
            default:
                std::abort();
            }
        }

        case CoroutineStatus::PauseOnRunning: {
            switch (status) {
            case CoroutineStatus::Running:
                newStatus = CoroutineStatus::Paused;
                break;
            case CoroutineStatus::Resumed:
                newStatus = CoroutineStatus::Suspended;
                break;
            default:
                std::abort();
            }
            break;
        }

        case CoroutineStatus::Paused: {
            switch (status) {
            case CoroutineStatus::Resumed:
                newStatus = CoroutineStatus::Running;
                break;
            default:
                std::abort();
            }
            break;
        }

        default:
            std::abort();
        }
    } while (!m_status.compare_exchange_weak(currentStatus, newStatus));
    return currentStatus;
}

bool Coroutine::isDone() const noexcept
{
    return m_status == CoroutineStatus::Returned
        || m_status == CoroutineStatus::ReturnedCompleted
        || m_status == CoroutineStatus::FinalSuspended
        || m_status == CoroutineStatus::CompletedFinalSuspended;
}

bool Coroutine::isCancelled() const noexcept
{
    return m_cancelled;
}

void Coroutine::cancel() noexcept
{
    if (m_cancelled)
        return;
    m_cancelled = true;

    if (status() != CoroutineStatus::Suspended)
        return;

    if (m_awaiters.isEmpty())
        return;

    auto prevStatus = setStatus(CoroutineStatus::PauseOnRunning);
    if (prevStatus != CoroutineStatus::Suspended)
        return;

    bool foundBlockingAwaiter;
    do {
        foundBlockingAwaiter = false;
        while (auto* awaiter = m_awaiters.dequeue()) {
            if (!awaiter->maybeBlocked())
                continue;
            auto cancelled = Awaiter::cancel(awaiter);
            foundBlockingAwaiter = foundBlockingAwaiter || !cancelled;
        }
    } while (foundBlockingAwaiter && status() != CoroutineStatus::Paused);

    prevStatus = setStatus(CoroutineStatus::Resumed);
    (void)prevStatus;
    assert(prevStatus == CoroutineStatus::PauseOnRunning
        || prevStatus == CoroutineStatus::Paused);
}

bool Coroutine::shouldCancelAbandoned() const noexcept
{
    return m_cancelAbandoned;
}

void Coroutine::setCancelAbandoned(bool cancelAbandoned) noexcept
{
    m_cancelAbandoned = cancelAbandoned;
    if (cancelAbandoned) {
        if (!m_owner && currentCoroutine())
            setOwner(currentCoroutine());
    } else if (m_owner) {
        setOwner(nullptr);
    }
}

AsyncCountDownEvent& Coroutine::completionEvent() noexcept
{
    return m_completionEvent;
}

Coroutine* Coroutine::currentCoroutine() noexcept
{
    return ThreadPool::currentCoroutine();
}

void Coroutine::setOwner() noexcept
{
    if (currentCoroutine())
        setOwner(currentCoroutine());
}

void Coroutine::clearOwner() noexcept
{
    setOwner(nullptr);
}

void Coroutine::setOwner(Coroutine* newOwnerCoroutine) noexcept
{
    if (m_completionEvent.isZero())
        return;
    auto alreadyCompleted = m_completionEvent.countUp();
    if (alreadyCompleted) {
        (void)m_completionEvent.countDown();
        signalOwner();
        return;
    }
    if (newOwnerCoroutine) {
        assert(!newOwnerCoroutine->m_completionEvent.isZero());
        auto isUpFromZero = newOwnerCoroutine->m_completionEvent.countUp();
        (void)isUpFromZero;
        assert(!isUpFromZero);
#ifdef DEBUG
        newOwnerCoroutine->addWaitingOnCompletion(this);
#endif
    }

    auto* oldOwnerCoroutine = m_owner.exchange(newOwnerCoroutine);
    if (oldOwnerCoroutine)
        oldOwnerCoroutine->completionEvent().countDown();
    (void)m_completionEvent.countDown();
    if (m_completionEvent.isZero())
        signalOwner();
}

void Coroutine::signalOwner() noexcept
{
    auto* owner = m_owner.exchange(nullptr);
    if (owner) {
        assert(!owner->completionEvent().isZero());
        owner->completionEvent().countDown();
    }
}

void Coroutine::addAwaiter(Awaiter* awaiter) noexcept
{
    m_awaiters.enqueue(awaiter);
}

void Coroutine::removeAwaiter(Awaiter* awaiter) noexcept
{
    bool found = m_awaiters.remove(awaiter);
    (void)found;
    assert(found || isCancelled());
}

void Coroutine::setCancelled() noexcept
{
    m_cancelled = true;
}

ThreadPoolKind Coroutine::threadPoolKind() const noexcept
{
    return m_threadPoolKind;
}

ThreadPoolKind Coroutine::determineThreadPoolKind(ThreadPoolKind requestedThreadPoolKind) noexcept
{
    if (requestedThreadPoolKind == ThreadPoolKind::Current) {
        if (Coroutine::currentCoroutine())
            return Coroutine::currentCoroutine()->threadPoolKind();
        if (ThreadPool::currentThreadPool())
            return ThreadPool::currentThreadPool()->kind();
        return ThreadPoolKind::Default;
    }
    return requestedThreadPoolKind;
}

ThreadPoolKind Coroutine::currentThreadPoolKind() noexcept
{
    return !ThreadPool::currentThreadPool() ? ThreadPoolKind::Current : ThreadPool::currentThreadPool()->kind();
}

#ifdef DEBUG

void Coroutine::addWaitingOnCompletion(Coroutine* coroutine) noexcept
{
    m_waitingOnCompletions.push_back(coroutine);
}

#endif

}
