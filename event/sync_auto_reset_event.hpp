//
// Created by irantha on 5/4/23.
//

#ifndef LEVELZ_SYNC_AUTO_RESET_EVENT_HPP
#define LEVELZ_SYNC_AUTO_RESET_EVENT_HPP

#include <chrono>
#include <condition_variable>
#include <mutex>

#include "task/simple_task.hpp"

namespace Levelz::Async {

template <typename, ThreadPoolKind>
struct Task;

struct SyncAutoResetEvent {
    explicit SyncAutoResetEvent(bool initiallySet = false) noexcept;
    void asyncSet() noexcept;
    [[nodiscard]] bool trySet() noexcept;
    void set() noexcept;
    void wait() noexcept;
    void waitFor(std::chrono::duration<uint64_t, std::chrono::nanoseconds::period> duration) noexcept;
    [[nodiscard]] bool isSet() const noexcept;

private:
    enum class State {
        Set,
        NotSet,
        None
    };

    BackgroundSimpleTask asyncSetTask();

    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::atomic<State> m_state;
};

}

#endif // LEVELZ_SYNC_AUTO_RESET_EVENT_HPP
