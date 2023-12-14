//
// Created by irantha on 6/11/23.
//

#ifndef LEVELZ_COROUTINE_HPP
#define LEVELZ_COROUTINE_HPP

#include <atomic>
#include <coroutine>
#ifdef DEBUG
#include <vector>
#endif

#include "event/async_countdown_event.hpp"
#include "task_kind.hpp"
#include "thread_pool_kind.hpp"
#include "awaiter_kind.hpp"
#include "fifo_wait_list.hpp"

namespace Levelz::Async {

enum class CoroutineStatus {
    NotStarted,
    Running,
    Paused,
    Suspended,
    PauseOnRunning,
    Yielded,
    Returned,
    Completed, // action
    Abandoned,
    Resumed, // action
    ReturnedCompleted, // Return value or exception -> child tasks complete
    YieldedSuspended, // Yield -> Yield Suspend
    FinalSuspended, // Return value or exception -> final suspend
    AbandonedFinalSuspended, // Returned + Final suspended + abandoned
    AbandonedYieldSuspended, // yielded + yield suspended + abandoned
    AbandonedCompleted, // YieldedCompleted or ReturnedCompleted -> abandoned
    CompletedFinalSuspended, // returned + final suspended + child tasks completed
};

struct Coroutine {
    ~Coroutine();
    Coroutine(const Coroutine&) = delete;
    Coroutine(Coroutine&&) = delete;
    Coroutine& operator=(const Coroutine&) = delete;
    Coroutine& operator=(Coroutine&&) = delete;

    void schedule() noexcept;

    [[nodiscard]] bool validate() const noexcept;
    CoroutineStatus status() const noexcept;
    bool isDone() const noexcept;
    bool isCancelled() const noexcept;
    void setCancelled() noexcept;
    void cancel() noexcept;
    bool shouldCancelAbandoned() const noexcept;
    static Coroutine* currentCoroutine() noexcept;
    void setCancelAbandoned(bool cancelAbandoned) noexcept;
    ThreadPoolKind threadPoolKind() const noexcept;
    static ThreadPoolKind currentThreadPoolKind() noexcept;
    AsyncCountDownEvent& completionEvent() noexcept;
    void setNext(Coroutine* nextOp) noexcept;
    Coroutine* next() const noexcept;

#ifdef DEBUG
    void addWaitingOnCompletion(Coroutine* coroutine) noexcept;
#endif

private:
    friend struct BasePromise;
    friend struct TaskPromiseYieldSuspendAwaiter;
    friend struct TaskPromiseFinalSuspendAwaiter;
    friend struct ThreadPool;
    friend struct Awaiter;
    template <ThreadPoolKind>
    friend struct SimpleTaskPromise;
    template <typename, ThreadPoolKind>
    friend struct AsyncTaskPromiseBase;
    template <typename, ThreadPoolKind>
    friend struct SyncTaskPromiseBase;
    template <typename, ThreadPoolKind>
    friend struct TaskPromiseBase;
    template <typename>
    friend struct BaseTask;
    friend struct TaskAwaiterBase;

    void resume();
    CoroutineStatus setStatus(CoroutineStatus status, bool isFinalAwaiter = false) noexcept;
    void justSetStatus(CoroutineStatus newStatus, CoroutineStatus expectedCurrentStatus) noexcept;
    Coroutine(std::coroutine_handle<> coroutine, bool cancelAbandoned, TaskKind taskKind, ThreadPoolKind threadPoolKind) noexcept;
    std::coroutine_handle<> handle() noexcept;
    static ThreadPoolKind determineThreadPoolKind(ThreadPoolKind requestedThreadPoolKind) noexcept;
    void setOwner(Coroutine* newOwnerCoroutine) noexcept;
    void signalOwner() noexcept;
    void setOwner() noexcept;
    void clearOwner() noexcept;
    void addAwaiter(Awaiter* awaiter) noexcept;
    void removeAwaiter(Awaiter* awaiter) noexcept;

    std::atomic<CoroutineStatus> m_status;
    std::atomic<bool> m_cancelled;
    const std::coroutine_handle<> m_handle;
    std::atomic<Coroutine*> m_next;
    std::atomic<bool> m_cancelAbandoned;
    AsyncCountDownEvent m_completionEvent;
    std::atomic<Coroutine*> m_owner;
    ConcurrentFifoList<Awaiter> m_awaiters;
    const ThreadPoolKind m_threadPoolKind;
#ifdef DEBUG
    // m_waitingOnCompletions's Coroutine pointers may be invalid
    std::vector<Coroutine*> m_waitingOnCompletions;
    const TaskKind m_taskKind;
#endif
};

}

#endif // LEVELZ_COROUTINE_HPP
