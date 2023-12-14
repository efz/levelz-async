//
// Created by irantha on 5/11/23.
//

#ifndef LEVELZ_COUNTED_HPP
#define LEVELZ_COUNTED_HPP

#include <atomic>

namespace Levelz::Async::Test {

struct Counted {
    static std::atomic<int> s_defaultConstructionCount;
    static std::atomic<int> s_copyConstructionCount;
    static std::atomic<int> s_copyAssignmentCount;
    static std::atomic<int> s_moveConstructionCount;
    static std::atomic<int> s_moveAssignmentCount;
    static std::atomic<int> s_destructionCount;

    int id;

    static void resetCounts()
    {
        s_defaultConstructionCount = 0;
        s_copyConstructionCount = 0;
        s_moveConstructionCount = 0;
        s_destructionCount = 0;
        s_copyAssignmentCount = 0;
        s_moveAssignmentCount = 0;
    }

    static int constructionCount()
    {
        return s_defaultConstructionCount
            + s_copyConstructionCount
            + s_moveConstructionCount;
    }

    static int activeCount()
    {
        return constructionCount() - s_destructionCount;
    }

    Counted()
        : id(s_defaultConstructionCount++)
    {
    }

    Counted(const Counted& other)
        : id(other.id)
    {
        ++s_copyConstructionCount;
    }

    Counted& operator=(const Counted& other)
    {
        id = other.id;
        s_copyAssignmentCount++;
        return *this;
    }

    Counted& operator=(Counted&& other) noexcept
    {
        id = other.id;
        other.id = -1;
        s_moveAssignmentCount++;
        return *this;
    }

    Counted(Counted&& other) noexcept
        : id(other.id)
    {
        ++s_moveConstructionCount;
        other.id = -1;
    }

    ~Counted()
    {
        ++s_destructionCount;
    }
};

}

#endif // LEVELZ_COUNTED_HPP
