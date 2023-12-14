//
// Created by irantha on 6/24/23.
//

#include "async_barrier.hpp"

namespace Levelz::Async {

AsyncBarrier::~AsyncBarrier()
{
    cancel();
    m_asyncScope.waitTillEmpty();
}

bool AsyncBarrier::arriveAndWait(Coroutine* awaiter) noexcept
{
    auto scopeTracker = m_asyncScope.onEnter();

    auto count = m_count.load();
    assert(!awaiter->next());
    if (m_canceled) {
        awaiter->setCancelled();
        return false;
    }
    m_waitList.enqueue(awaiter);
    if (m_canceled) {
        while (auto* coroutine = m_waitList.dequeue()) {
            coroutine->setCancelled();
            coroutine->schedule();
        }
        return true;
    }

    do {
        assert(count < m_capacity);
        if (count == m_capacity - 1) {
            auto success = arriveAndRelease(awaiter);
            assert(success);
            (void)success;
            return false;
        }
        assert(count < m_capacity);
    } while (!m_count.compare_exchange_weak(count, count + 1));

    assert(m_count.load() < m_capacity);
    return true;
}

bool AsyncBarrier::arriveAndRelease(Coroutine* awaiter) noexcept
{
    auto scopeTracker = m_asyncScope.onEnter();

    if (m_count.load() != m_capacity - 1)
        return false;

    int dequeCount = !awaiter ? m_capacity - 1 : m_capacity;
    m_count = 0;
    for (int i = 0; i < dequeCount; i++) {
        auto* op = m_waitList.dequeue();
        assert(!op->next());
        if (awaiter && op == awaiter)
            continue;
        op->schedule();
    }
    return true;
}

bool AsyncBarrier::isWaitListEmpty() const noexcept
{
    return m_waitList.isEmpty();
}

void AsyncBarrier::cancel() noexcept
{
    if (m_canceled)
        return;
    auto scopeTracker = m_asyncScope.onEnter();

    m_canceled = true;
    while (auto* coroutine = m_waitList.dequeue()) {
        coroutine->setCancelled();
        coroutine->schedule();
    }
}

bool AsyncBarrier::isCanceled() const noexcept
{
    return m_canceled;
}

void AsyncBarrier::reset() noexcept
{
    m_canceled = false;
    m_count = 0;
}

uint64_t AsyncBarrier::waitListedCount() const noexcept
{
    return m_count;
}

}
