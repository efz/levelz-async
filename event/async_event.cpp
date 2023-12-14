//
// Created by irantha on 6/23/23.
//

#include "async_event.hpp"
#include "task/simple_task.hpp"

namespace Levelz::Async {

AsyncEvent::AsyncEvent(bool initiallySet) noexcept
    : m_countDownEvent(initiallySet, 1)
{
}

void AsyncEvent::signal() noexcept
{
    assert(m_countDownEvent.count() <= 1 && m_countDownEvent.count() >= 0);
    m_countDownEvent.countDown();
    assert(m_countDownEvent.count() <= 1 && m_countDownEvent.count() >= 0);
}

void AsyncEvent::reset() noexcept
{
    assert(m_countDownEvent.count() <= 1 && m_countDownEvent.count() >= 0);
    m_countDownEvent.countUp();
    assert(m_countDownEvent.count() <= 1 && m_countDownEvent.count() >= 0);
}

void AsyncEvent::enqueue(AsyncEvent& event) noexcept
{
    assert(m_countDownEvent.count() <= 1 && m_countDownEvent.count() >= 0);

    m_countDownEvent.enqueue(event.m_countDownEvent);
}

void AsyncEvent::enqueue(SyncManualResetEvent& event) noexcept
{
    assert(m_countDownEvent.count() <= 1 && m_countDownEvent.count() >= 0);

    m_countDownEvent.enqueue(event);
}

bool AsyncEvent::isWaitListEmpty() const noexcept
{
    return m_countDownEvent.isWaitListEmpty();
}

bool AsyncEvent::isSignaled() const noexcept
{
    return m_countDownEvent.isZero();
}

uint64_t AsyncEvent::waitListedCount() const noexcept
{
    return m_countDownEvent.waitListedCount();
}

bool AsyncEvent::enqueue(Coroutine& coroutine) noexcept
{
    return m_countDownEvent.enqueue(coroutine);
}

bool AsyncEvent::remove(Coroutine* coroutineToRemove) noexcept
{
    return m_countDownEvent.remove(coroutineToRemove);
}

}
