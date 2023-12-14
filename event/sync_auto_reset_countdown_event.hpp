//
// Created by irantha on 5/17/23.
//

#ifndef LEVELZ_SYNC_AUTO_RESET_COUNT_DOWN_EVENT_HPP
#define LEVELZ_SYNC_AUTO_RESET_COUNT_DOWN_EVENT_HPP

#include <condition_variable>
#include <mutex>

namespace Levelz::Async {

struct SyncAutoResetCountDownEvent {
    explicit SyncAutoResetCountDownEvent(int initialCount = 1);
    void set();
    void wait();

private:
    std::mutex m_mutex;
    std::condition_variable m_cv;
    const int m_initialCount;
    int m_count;
};

}

#endif // LEVELZ_SYNC_AUTO_RESET_COUNT_DOWN_EVENT_HPP
