//
// Created by irantha on 10/26/23.
//

#ifndef LEVELZ_ASYNC_SPIN_WAIT_HPP
#define LEVELZ_ASYNC_SPIN_WAIT_HPP

#include <cinttypes>

namespace Levelz::Async {

struct AsyncSpinWait {
    AsyncSpinWait() noexcept;
    [[nodiscard]] bool willNextSpinYield() const noexcept;
    void reset() noexcept;
    void spinOne() noexcept;

private:
    uint64_t m_count;
    constexpr static uint64_t s_yieldThreshold = 4;
};

}

#endif // LEVELZ_ASYNC_SPIN_WAIT_HPP
