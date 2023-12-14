//
// Created by irantha on 11/3/23.
//

#include "spin_wait.hpp"

namespace Levelz::Async {

SpinWait::SpinWait() noexcept
    : m_count { 0 }
{
    reset();
}

[[nodiscard]] bool SpinWait::willNextSpinYield() const noexcept
{
    return m_count % s_yieldThreshold == 0;
}

void SpinWait::reset() noexcept
{
    m_count = 0;
}

void SpinWait::spinOne() noexcept
{
    if (willNextSpinYield())
        std::this_thread::yield();

    ++m_count;
}

}