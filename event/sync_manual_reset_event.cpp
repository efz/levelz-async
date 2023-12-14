//
// Created by irantha on 5/5/23.
//

#include "sync_manual_reset_event.hpp"

namespace Levelz::Async {

SyncManualResetEvent::SyncManualResetEvent(bool initiallySet)
    : m_isSet { initiallySet }
    , m_mutex {}
    , m_cv {}
{
}

void SyncManualResetEvent::set()
{
    std::scoped_lock lock { m_mutex };
    if (!m_isSet) {
        m_isSet = true;
        m_cv.notify_one();
    }
}

void SyncManualResetEvent::reset()
{
    std::scoped_lock lock { m_mutex };
    m_isSet = false;
}

void SyncManualResetEvent::wait()
{
    std::unique_lock lock { m_mutex };
    if (m_isSet)
        return;
    m_cv.wait(lock, [this] { return m_isSet; });
}

bool SyncManualResetEvent::isSet() const noexcept
{
    return m_isSet;
}

}