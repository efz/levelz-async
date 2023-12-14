//
// Created by irantha on 5/4/23.
//

#include <cassert>

#include "task/simple_task.hpp"
#include "sync_auto_reset_event.hpp"

namespace Levelz::Async {

SyncAutoResetEvent::SyncAutoResetEvent(bool initiallySet) noexcept
    : m_state { initiallySet ? State::Set : State::None }
    , m_mutex {}
    , m_cv {}
{
}

void SyncAutoResetEvent::set() noexcept
{
    auto oldState = m_state.exchange(State::Set);
    if (oldState != State::NotSet)
        return;

    std::unique_lock lock { m_mutex };
    m_cv.notify_one();
}

BackgroundSimpleTask SyncAutoResetEvent::asyncSetTask()
{
    set();
    co_return;
}

void SyncAutoResetEvent::asyncSet() noexcept
{
    if (m_mutex.try_lock()) {
        std::unique_lock lock { m_mutex, std::adopt_lock };
        (void)m_state.exchange(State::Set);
        m_cv.notify_one();
        return;
    }

    BackgroundSimpleTask simpleTask = asyncSetTask();
    simpleTask.coroutine().schedule();
    simpleTask.clear();
}

void SyncAutoResetEvent::wait() noexcept
{
    State prevState = State::None;
    if (!m_state.compare_exchange_strong(prevState, State::NotSet)) {
        assert(prevState == State::Set);
        m_state = State::None;
        return;
    }

    std::unique_lock lock { m_mutex };
    if (m_state == State::Set) {
        m_state = State::None;
        return;
    }
    m_cv.wait(lock, [this] { return m_state == State::Set; });
    assert(m_state == State::Set);
    m_state = State::None;
}

void SyncAutoResetEvent::waitFor(std::chrono::duration<uint64_t, std::chrono::nanoseconds::period> duration) noexcept
{
    State prevState = State::None;
    if (!m_state.compare_exchange_strong(prevState, State::NotSet)) {
        assert(prevState == State::Set);
        m_state = State::None;
        return;
    }

    std::unique_lock lock { m_mutex };
    if (m_state == State::Set) {
        m_state = State::None;
        return;
    }
    m_cv.wait_for(lock, duration, [this] { return m_state == State::Set; });
    m_state = State::None;
}

bool SyncAutoResetEvent::isSet() const noexcept
{
    return m_state == State::Set;
}

}
