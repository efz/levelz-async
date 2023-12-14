//
// Created by irantha on 12/13/23.
//

#include "async_countdown_event.hpp"
#include "task/simple_task.hpp"

namespace Levelz::Async {

namespace {
    SimpleTask<> asyncCountDown(AsyncCountDownEvent& event)
    {
        event.countDown();
        co_return;
    }

    SimpleTask<> asyncSetSyncEvent(SyncManualResetEvent& event)
    {
        event.set();
        co_return;
    }
}

AsyncCountDownEvent::AsyncCountDownEvent(bool initiallyZero, int maxCount) noexcept
    : m_waitQueue {}
    , m_count { initiallyZero ? 0 : 1 }
    , m_asyncScope {}
    , m_maxCount { maxCount }
{
}

AsyncCountDownEvent::~AsyncCountDownEvent()
{
    while (auto* coroutine = m_waitQueue.dequeue()) {
        coroutine->setCancelled();
        coroutine->schedule();
    }

    m_asyncScope.waitTillEmpty();
}

bool AsyncCountDownEvent::countDown() noexcept
{
    auto scopeTracker = m_asyncScope.onEnter();
    assert(m_count >= 0 && m_count <= m_maxCount);

    int prevCount = m_count;
    do {
        assert(prevCount >= 0 && m_count <= m_maxCount);
        if (prevCount == 0)
            return false;
    } while (!m_count.compare_exchange_weak(prevCount, prevCount - 1));

    if (prevCount == 1) {
        resumeWaiting();
        return true;
    }
    return false;
}

bool AsyncCountDownEvent::countUp() noexcept
{
    assert(m_count >= 0 && m_count <= m_maxCount);

    int prevCount = m_count;
    do {
        assert(prevCount >= 0 && m_count <= m_maxCount);
        if (prevCount == m_maxCount)
            return false;
    } while (!m_count.compare_exchange_weak(prevCount, prevCount + 1));

    assert(prevCount >= 0);
    return prevCount == 0;
}

void AsyncCountDownEvent::resumeWaiting() noexcept
{
    assert(m_count >= 0 && m_count <= m_maxCount);

    while (auto* coroutine = m_waitQueue.dequeue()) {
        assert(!coroutine->next());
        if (!isZero())
            m_waitQueue.enqueue(coroutine);
        else
            coroutine->schedule();
        if (!isZero())
            break;
    }
}

bool AsyncCountDownEvent::enqueue(Coroutine& coroutine) noexcept
{
    assert(m_count >= 0 && m_count <= m_maxCount);

    if (isZero())
        return false;

    auto scopeTracker = m_asyncScope.onEnter();

    m_waitQueue.enqueue(&coroutine);

    if (isZero())
        resumeWaiting();

    return true;
}

void AsyncCountDownEvent::enqueue(AsyncCountDownEvent& event) noexcept
{
    assert(m_count >= 0 && m_count <= m_maxCount);

    auto simpleTask = asyncCountDown(event);
    enqueueAlways(simpleTask.coroutine());
    simpleTask.clear();
}

void AsyncCountDownEvent::enqueue(SyncManualResetEvent& event) noexcept
{
    assert(m_count >= 0 && m_count <= m_maxCount);

    auto simpleTask = asyncSetSyncEvent(event);
    enqueueAlways(simpleTask.coroutine());
    simpleTask.clear();
}

bool AsyncCountDownEvent::isWaitListEmpty() const noexcept
{
    return m_waitQueue.isEmpty();
}

bool AsyncCountDownEvent::isZero() const noexcept
{
    return m_count == 0;
}

void AsyncCountDownEvent::enqueueAlways(Coroutine& simpleTaskCoroutine) noexcept
{
    assert(m_count >= 0 && m_count <= m_maxCount);

    auto enqueued = enqueue(simpleTaskCoroutine);
    assert(enqueued || isZero());
    if (!enqueued) {
        assert(isZero());
        simpleTaskCoroutine.schedule();
    }
}

uint64_t AsyncCountDownEvent::count() const noexcept
{
    return m_count;
}

AsyncCountDownEvent::AsyncCountDownEventAwaiter::AsyncCountDownEventAwaiter(
    AsyncCountDownEvent& event, Coroutine& coroutine) noexcept
    : Awaiter { coroutine, AwaiterKind::Event }
    , m_event { event }
{
}

bool AsyncCountDownEvent::AsyncCountDownEventAwaiter::await_ready() noexcept
{
    auto suspensionAdvice = Awaiter::onReady();
    if (suspensionAdvice == Awaiter::SuspensionAdvice::shouldNotSuspend)
        return true;
    if (suspensionAdvice == Awaiter::SuspensionAdvice::shouldSuspend)
        return false;

    return m_event.isZero();
}

bool AsyncCountDownEvent::AsyncCountDownEventAwaiter::await_suspend(std::coroutine_handle<> awaitingCoroutineHandle) noexcept
{
    auto suspensionAdvice = Awaiter::onSuspend(awaitingCoroutineHandle);
    if (suspensionAdvice == Awaiter::SuspensionAdvice::shouldNotSuspend)
        return false;

    setMaybeBlocked(true);
    auto isWaitListed = m_event.enqueue(coroutine());
    setMaybeBlocked(isWaitListed);
    if (suspensionAdvice == Awaiter::SuspensionAdvice::shouldSuspend) {
        if (!isWaitListed)
            coroutine().schedule();
        return true;
    }

    return isWaitListed;
}

void AsyncCountDownEvent::AsyncCountDownEventAwaiter::await_resume()
{
    Awaiter::onResume();
}

bool AsyncCountDownEvent::AsyncCountDownEventAwaiter::cancel() noexcept
{
    auto& coroutine = Awaiter::coroutine();
    assert(coroutine.isCancelled());
    assert(coroutine.status() == CoroutineStatus::PauseOnRunning || coroutine.status() == CoroutineStatus::Paused);
    auto removed = m_event.remove(&coroutine);
    if (removed)
        coroutine.schedule();
    return removed;
}

bool AsyncCountDownEvent::remove(Coroutine* coroutineToRemove) noexcept
{
    if (isZero())
        return false;

    auto removed = m_waitQueue.remove(coroutineToRemove);
    if (isZero())
        resumeWaiting();
    return removed;
}

uint64_t AsyncCountDownEvent::waitListedCount() const noexcept
{
    return m_waitQueue.count();
}

}
