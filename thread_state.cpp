//
// Created by irantha on 6/11/23.
//

#include "thread_state.hpp"
#include "coroutine.hpp"
#include "thread_pool.hpp"

namespace Levelz::Async {

ThreadState::ThreadState() noexcept
    : m_localQueue {}
    , m_isSleeping { false }
    , m_rng { std::random_device {}() }
    , m_threadIndex { -1 }
    , m_chainedExecutionAllowance { s_maxChainedExecutionAllowance }
{
}

bool ThreadState::wakeUpIfSleeping()
{
    if (!m_isSleeping)
        return false;
    m_wakeUpEvent.set();
    return true;
}

bool ThreadState::tryWakeUpIfSleeping() noexcept
{
    if (!m_isSleeping)
        return false;
    return m_wakeUpEvent.trySet();
}

void ThreadState::sleepUntilWoken()
{
    m_wakeUpEvent.wait();
}

bool ThreadState::haveLocalWork() const noexcept
{
    return !m_localQueue.isEmpty();
}

void ThreadState::localEnqueue(Coroutine* scheduleOperation) noexcept
{
    m_localQueue.enqueue(scheduleOperation);
}

Coroutine* ThreadState::tryLocalPop() noexcept
{
    return m_localQueue.dequeue();
}

uint64_t ThreadState::rand()
{
    return m_rng();
}

void ThreadState::setSleeping(bool isSleeping) noexcept
{
    m_isSleeping = isSleeping;
}

bool ThreadState::isSleeping() const noexcept
{
    return m_isSleeping;
}

int ThreadState::threadIndex() const noexcept
{
    assert(m_threadIndex != -1);
    return m_threadIndex;
}

void ThreadState::setThreadIndex(int threadIndex) noexcept
{
    assert(m_threadIndex == -1);
    m_threadIndex = threadIndex;
}

int ThreadState::chainedExecutionAllowance() const noexcept
{
    return m_chainedExecutionAllowance;
}

void ThreadState::setChainedExecutionAllowance(int count) noexcept
{
    m_chainedExecutionAllowance = std::min(count, s_maxChainedExecutionAllowance);
}

void ThreadState::recordChainedExecution() noexcept
{
    assert(m_chainedExecutionAllowance > 0);
    m_chainedExecutionAllowance--;
}

}
