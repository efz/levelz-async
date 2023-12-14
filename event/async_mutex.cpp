//
// Created by irantha on 6/23/23.
//

#include "async_mutex.hpp"
#include "async_spin_wait.hpp"

namespace Levelz::Async {

AsyncMutex::~AsyncMutex()
{
    cancel();
    if (m_ownerCoroutine) {
        AsyncSpinWait spinWait;
        while (m_ownerCoroutine)
            spinWait.spinOne();
    }
    assert(!m_ownerCoroutine);
    assert(m_waitList.isEmpty());
    m_asyncScope.waitTillEmpty();
}

bool AsyncMutex::tryLock(Coroutine* awaiterCoroutine)
{
    assert(awaiterCoroutine);
    auto* ownerCoroutine = m_ownerCoroutine.load();
    do {
        if (ownerCoroutine)
            return false;
    } while (!m_ownerCoroutine.compare_exchange_weak(ownerCoroutine, awaiterCoroutine));
    return true;
}

void AsyncMutex::lock(Coroutine* awaiterCoroutine)
{
    auto scopeTracker = m_asyncScope.onEnter();

    auto* ownerCoroutine = m_ownerCoroutine.load();
    auto* nextOwnerCoroutine = awaiterCoroutine;
    do {
        if (ownerCoroutine) {
            m_waitList.enqueue(nextOwnerCoroutine);
            ownerCoroutine = m_ownerCoroutine.load();
            if (ownerCoroutine)
                return;
            else
                nextOwnerCoroutine = m_waitList.dequeue();
        }
        if (!nextOwnerCoroutine)
            return;
        assert(!ownerCoroutine);
    } while (!m_ownerCoroutine.compare_exchange_weak(ownerCoroutine, nextOwnerCoroutine));

    assert(nextOwnerCoroutine);
    nextOwnerCoroutine->schedule();
}

void AsyncMutex::unlock(Coroutine* awaiterCoroutine)
{
    assert(awaiterCoroutine->validate());
    auto* ownerCoroutine = m_ownerCoroutine.load();
    if (ownerCoroutine != awaiterCoroutine) {
        if (isCancelled())
            awaiterCoroutine->setCancelled();
        return;
    }
    if (isCancelled()) {
        ownerCoroutine->setCancelled();
        while (auto nextOwnerCoroutine = m_waitList.dequeue()) {
            nextOwnerCoroutine->setCancelled();
            nextOwnerCoroutine->schedule();
        }
        m_ownerCoroutine = nullptr;
        while (auto nextOwnerCoroutine = m_waitList.dequeue()) {
            nextOwnerCoroutine->setCancelled();
            nextOwnerCoroutine->schedule();
        }
        return;
    }

    auto* nextOwnerCoroutine = m_waitList.dequeue();
    if (!nextOwnerCoroutine) {
        m_ownerCoroutine = nullptr;
        nextOwnerCoroutine = m_waitList.dequeue();
        if (!nextOwnerCoroutine)
            return;
        lock(nextOwnerCoroutine);
    } else {
        m_ownerCoroutine = nextOwnerCoroutine;
        assert(!nextOwnerCoroutine->next());
        nextOwnerCoroutine->schedule();
    }
}

bool AsyncMutex::isLocked() const noexcept
{
    return m_ownerCoroutine != nullptr;
}

bool AsyncMutex::isWaitListEmpty() const noexcept
{
    return m_waitList.isEmpty();
}

void AsyncMutex::cancel() noexcept
{
    m_cancelled = true;

    while (auto* coroutine = m_waitList.dequeue()) {
        coroutine->setCancelled();
        coroutine->schedule();
    }
}

bool AsyncMutex::isCancelled() const noexcept
{
    return m_cancelled;
}

bool AsyncMutex::remove(Coroutine* coroutineToRemove) noexcept
{
    auto removed = m_waitList.remove(coroutineToRemove);
    if (!m_ownerCoroutine)
        if (auto* waitingCoroutine = m_waitList.dequeue())
            lock(waitingCoroutine);

    return removed;
}

uint64_t AsyncMutex::waitListedCount() const noexcept
{
    return m_waitList.count();
}

}
