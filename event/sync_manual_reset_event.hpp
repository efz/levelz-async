//
// Created by irantha on 5/5/23.
//

#ifndef LEVELZ_MANUAL_RESET_EVENT_HPP
#define LEVELZ_MANUAL_RESET_EVENT_HPP

#include <condition_variable>
#include <mutex>

namespace Levelz::Async {

struct SyncManualResetEvent {
    explicit SyncManualResetEvent(bool initiallySet = false);
    void set();
    void reset();
    void wait();
    [[nodiscard]] bool isSet() const noexcept;

private:
    std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_isSet;
};

}

#endif // LEVELZ_MANUAL_RESET_EVENT_HPP
