//
// Created by irantha on 5/5/23.
//

#ifndef LEVELZ_THREAD_STATE_HPP
#define LEVELZ_THREAD_STATE_HPP

#include <atomic>
#include <cassert>
#include <mutex>
#include <random>
#include <thread>

#include "event/sync_auto_reset_event.hpp"
#include "coroutine.hpp"
#include "fifo_wait_list.hpp"

namespace Levelz::Async {

struct ThreadState {
    ThreadState() noexcept;

private:
    friend struct ThreadPool;

    bool wakeUpIfSleeping();
    [[nodiscard]] bool tryWakeUpIfSleeping() noexcept;
    void sleepUntilWoken();
    bool haveLocalWork() const noexcept;
    void localEnqueue(Coroutine* operation) noexcept;
    Coroutine* tryLocalPop() noexcept;
    uint64_t rand();
    void setSleeping(bool isSleeping) noexcept;
    bool isSleeping() const noexcept;
    int threadIndex() const noexcept;
    void setThreadIndex(int threadIndex) noexcept;

    int chainedExecutionAllowance() const noexcept;
    void setChainedExecutionAllowance(int count) noexcept;
    void recordChainedExecution() noexcept;

    int m_threadIndex {};
    FifoWaitList m_localQueue;
    std::atomic<bool> m_isSleeping;
    SyncAutoResetEvent m_wakeUpEvent;
    std::default_random_engine m_rng;
    int m_chainedExecutionAllowance;

    static constexpr int s_maxChainedExecutionAllowance = 100;
};

}

#endif // LEVELZ_THREAD_STATE_HPP
