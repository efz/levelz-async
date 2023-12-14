//
// Created by irantha on 10/26/23.
//

#include "async_spin_wait.hpp"
#include "thread_pool.hpp"

namespace Levelz::Async {

AsyncSpinWait::AsyncSpinWait() noexcept
    : m_count { 0 }
{
    reset();
}

bool AsyncSpinWait::willNextSpinYield() const noexcept
{
    return m_count % s_yieldThreshold == 0;
}

void AsyncSpinWait::reset() noexcept
{
    m_count = 0;
}

void AsyncSpinWait::spinOne() noexcept
{
    if (willNextSpinYield())
        ThreadPool::yield();

    ++m_count;
}

}