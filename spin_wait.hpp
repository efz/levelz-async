//
// Created by irantha on 5/3/23.
//

#ifndef LEVELZ_SPIN_WAIT_HPP
#define LEVELZ_SPIN_WAIT_HPP

#include <cinttypes>
#include <thread>

namespace Levelz::Async {

struct SpinWait {
    SpinWait() noexcept;
    [[nodiscard]] bool willNextSpinYield() const noexcept;
    void reset() noexcept;
    void spinOne() noexcept;

private:
    uint64_t m_count;
    constexpr static uint64_t s_yieldThreshold = 16;
};

}

#endif // LEVELZ_SPIN_WAIT_HPP
