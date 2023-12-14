//
// Created by irantha on 5/17/23.
//

#include "sync_auto_reset_countdown_event.hpp"

namespace Levelz::Async {

SyncAutoResetCountDownEvent::SyncAutoResetCountDownEvent(int initialCount)
    : m_mutex {}
    , m_cv {}
    , m_initialCount { initialCount }
    , m_count { initialCount }
{
}

void SyncAutoResetCountDownEvent::set()
{
    std::scoped_lock lock { m_mutex };
    if (m_count > 0)
        m_count--;

    if (m_count == 0)
        m_cv.notify_one();
}

void SyncAutoResetCountDownEvent::wait()
{
    std::unique_lock lock { m_mutex };
    while (m_count != 0) {
        m_cv.wait(lock);
    }
    m_count = m_initialCount;
}

}
