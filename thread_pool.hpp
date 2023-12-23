//
// Created by irantha on 5/3/23.
//

#ifndef LEVELZ_THREAD_POOL_HPP
#define LEVELZ_THREAD_POOL_HPP

#include <atomic>
#include <coroutine>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "coroutine.hpp"
#include "fifo_wait_list.hpp"
#include "thread_pool_kind.hpp"
#include "thread_state.hpp"

namespace Levelz::Async {

struct ThreadPool {
    ThreadPool(int threadCount, ThreadPoolKind kind);

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    ~ThreadPool();

    void shutdown();
    void shutdownImmediately();
    int threadCount() const noexcept;
    bool haveWork() const noexcept;
    int sleepingThreadCount() const noexcept;
    bool noLocalWork() const noexcept;
    ThreadPoolKind kind() const noexcept;

    static int maxThreadCount() noexcept;
    static void waitForAllThreadsIdle();
    static bool isShutdownRequested() noexcept;
    static bool isImmediateShutdownRequested() noexcept;
    static void shutdownAll();
    static ThreadPoolKind currentThreadPoolKind() noexcept;

private:
    enum class State {
        NotStarted,
        Started,
        ShuttingDown,
        ShuttingDownImmediately,
        Terminated
    };

    friend struct ThreadPoolAwaiter;
    friend struct Coroutine;
    friend struct TaskPromiseFinalSuspendAwaiter;
    friend struct AsyncSpinWait;
    friend struct Awaiter;

    void setSleeping(bool isSleeping);
    void runWorkerThread(int threadIndex) noexcept;
    void shutdown(State state);

    void globalEnqueue(Coroutine* operation) noexcept;
    Coroutine* tryGlobalDequeue() noexcept;
    Coroutine* tryStealFromOtherThread() noexcept;
    static void yield();

    void wakeOneThread() noexcept;
    [[nodiscard]] bool wakeOneThread(bool doImmediateWakeUp) noexcept;
    void processPendingWakeUps() noexcept;
    Coroutine* tryGetRemote() noexcept;
    Coroutine* tryGetWork() noexcept;
    static void resume(Coroutine* coroutine, int chainedExecutionAllowance);
    void scheduleOnThreadPool(Coroutine* coroutine) noexcept;
    static bool canDoChainedExecution() noexcept;
    static void recordChainedExecution() noexcept;
    static Coroutine* currentCoroutine() noexcept;
    static void setCurrentCoroutine(Coroutine* coroutine) noexcept;
    static ThreadPool& threadPool(ThreadPoolKind kind) noexcept;
    static ThreadPool* currentThreadPool() noexcept;

    static thread_local ThreadState* s_currentState;
    static thread_local ThreadPool* s_currentThreadPool;
    static thread_local Coroutine* s_currentCoroutine;

    std::atomic<State> m_state;
    const int m_threadCount;
    const std::unique_ptr<ThreadState[]> m_threadStates;
    std::vector<std::thread> m_threads;

    std::atomic<int> m_mayBeSleepingThreadCount;
    std::atomic<int> m_sleepingThreadCount;

    FifoWaitList m_globalQueue;
    const bool m_noLocalWork;
    const ThreadPoolKind m_kind;
    std::atomic<int> m_pendingWakeUpRequestCount;

    static constexpr int s_numRemoteWorksChecksBeforeSleep = 32;
};

}

#endif // LEVELZ_THREAD_POOL_HPP
