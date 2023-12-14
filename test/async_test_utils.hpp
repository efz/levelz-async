//
// Created by irantha on 6/23/23.
//

#ifndef LEVELZ_ASYNC_TEST_UTILS_HPP
#define LEVELZ_ASYNC_TEST_UTILS_HPP

#include <random>

#include "async_spin_wait.hpp"

#define REPEAT_COUNT 1'000
#define REPEAT_HEADER for (unsigned long long zz11k = 0; zz11k < (REPEAT_COUNT); zz11k++) {
#define REPEAT_FOOTER }

namespace Levelz::Async::Test {

struct AsyncTestUtils {
    static void randomSpinWait(uint64_t maxSpinCount = 100)
    {
        std::default_random_engine rng { std::random_device {}() };
        AsyncSpinWait spinWait;
        auto spinCount = rng() % maxSpinCount;
        for (int ii = 0; ii < spinCount; ii++)
            spinWait.spinOne();
    }
};

}

#endif // LEVELZ_ASYNC_TEST_UTILS_HPP
