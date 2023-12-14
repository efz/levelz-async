//
// Created by irantha on 11/3/23.
//

#include <cassert>

#include "spin_wait.hpp"
#include "async_scope.hpp"

namespace Levelz::Async {
AsyncScope::AsyncScope() noexcept
    : m_count { 0 }
{
}

AsyncScope::~AsyncScope()
{
    waitTillEmpty();
}

AsyncScope::AsyncScopeTracker AsyncScope::onEnter() noexcept
{
    assert(m_count >= 0);
    auto entryCount = m_count.fetch_add(1);
    assert(entryCount >= 0);
    return { this, entryCount };
}

bool AsyncScope::isEmpty() const noexcept
{
    return m_count == 0;
}

void AsyncScope::waitTillEmpty() const noexcept
{
    if (isEmpty())
        return;

    SpinWait spinWait;
    while (!isEmpty())
        spinWait.spinOne();
}

void AsyncScope::onExit(uint8_t entryCount) noexcept
{
    assert(entryCount >= 0);
    (void)entryCount;
    assert(m_count > 0);
    int exitCount = m_count.fetch_sub(1);
    assert(exitCount > 0);
    (void)exitCount;
}

}