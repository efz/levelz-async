//
// Created by irantha on 5/7/23.
//

#ifndef LEVELZ_TASK_AWAITER_HPP
#define LEVELZ_TASK_AWAITER_HPP

#include <coroutine>

#include "awaiter.hpp"
#include "coroutine.hpp"
#include "promise_error.hpp"

namespace Levelz::Async {

template <typename, ThreadPoolKind>
struct Task;

struct TaskAwaiterBase : Awaiter {
    TaskAwaiterBase(std::atomic<Coroutine*>& continuation, Coroutine& coroutine, Coroutine& taskCoroutine) noexcept
        : Awaiter { coroutine, AwaiterKind::Task }
        , m_taskCoroutine { taskCoroutine }
        , m_continuation { continuation }
    {
    }

    [[nodiscard]] bool await_ready() noexcept
    {
        auto suspensionAdvice = Awaiter::onReady();
        (void)suspensionAdvice;
        if (suspensionAdvice == SuspensionAdvice::shouldNotSuspend)
            return true;
        if (m_taskCoroutine.isDone())
            return true;
        return false;
    }

    std::coroutine_handle<> await_suspend(std::coroutine_handle<> handle) noexcept
    {
        auto suspensionAdvice = Awaiter::onSuspend(handle);

        if (suspensionAdvice == Awaiter::SuspensionAdvice::shouldNotSuspend)
            return handle;

        auto taskStatus = m_taskCoroutine.status();
        if (taskStatus == CoroutineStatus::NotStarted)
            m_taskCoroutine.justSetStatus(CoroutineStatus::Suspended, CoroutineStatus::NotStarted);
        else if (taskStatus == CoroutineStatus::YieldedSuspended)
            m_taskCoroutine.justSetStatus(CoroutineStatus::Suspended, CoroutineStatus::YieldedSuspended);
        else if (m_taskCoroutine.isDone())
            return handle;
        else
            std::abort(); // throw

        m_continuation = &coroutine();

        setMaybeBlocked(true);
        if (suspensionAdvice == Awaiter::SuspensionAdvice::shouldSuspend) {
            m_taskCoroutine.schedule();
            return std::noop_coroutine();
        }

        if (m_taskCoroutine.threadPoolKind() != ThreadPool::currentThreadPoolKind()) {
            m_taskCoroutine.schedule();
            return std::noop_coroutine();
        }

        return m_taskCoroutine.handle();
    }

    bool cancel() noexcept
    {
        auto& coroutine = Awaiter::coroutine();
        (void)coroutine;
        assert(coroutine.isCancelled());
        assert(coroutine.status() == CoroutineStatus::PauseOnRunning || coroutine.status() == CoroutineStatus::Paused);
        auto continuationCoroutine = m_continuation.exchange(nullptr);

        if (continuationCoroutine) {
            assert(continuationCoroutine->handle() == coroutine.handle());
            continuationCoroutine->schedule();
        }
        return continuationCoroutine != nullptr;
    }

private:
    std::atomic<Coroutine*>& m_continuation;
    Coroutine& m_taskCoroutine;
};

template <typename ValueType, ThreadPoolKind TPK>
struct TaskAwaiter : TaskAwaiterBase {
    TaskAwaiter(Task<ValueType, TPK>& task,
        std::atomic<Coroutine*>& continuation, Coroutine& coroutine, Coroutine& taskCoroutine) noexcept
        : TaskAwaiterBase { continuation, coroutine, taskCoroutine }
        , m_task { task }
    {
    }

    ValueType await_resume()
    {
        m_task.continuation().store(nullptr);
        onResume();
        return m_task.value();
    }

private:
    Task<ValueType, TPK>& m_task;
};

template <typename ValueType, ThreadPoolKind TPK>
struct TaskAwaiter<ValueType&, TPK> : TaskAwaiterBase {
    TaskAwaiter(Task<ValueType&, TPK>& task,
        std::atomic<Coroutine*>& continuation, Coroutine& coroutine, Coroutine& taskCoroutine) noexcept
        : TaskAwaiterBase { continuation, coroutine, taskCoroutine }
        , m_task { task }
    {
    }

    ValueType& await_resume()
    {
        m_task.continuation().store(nullptr);
        onResume();
        return m_task.value();
    }

private:
    Task<ValueType&, TPK>& m_task;
};

template <typename ValueType, ThreadPoolKind TPK>
struct TaskAwaiter<ValueType&&, TPK> : TaskAwaiterBase {
    TaskAwaiter(Task<ValueType&&, TPK>& task,
        std::atomic<Coroutine*>& continuation, Coroutine& coroutine, Coroutine& taskCoroutine) noexcept
        : TaskAwaiterBase { continuation, coroutine, taskCoroutine }
        , m_task { task }
    {
    }

    ValueType await_resume()
    {
        m_task.continuation().store(nullptr);
        onResume();
        return m_task.value();
    }

private:
    Task<ValueType&&, TPK>& m_task;
};

template <ThreadPoolKind TPK>
struct TaskAwaiter<void, TPK> : TaskAwaiterBase {
    TaskAwaiter(Task<void, TPK>& task,
        std::atomic<Coroutine*>& continuation, Coroutine& coroutine, Coroutine& taskCoroutine) noexcept
        : TaskAwaiterBase { continuation, coroutine, taskCoroutine }
        , m_task { task }
    {
    }

    void await_resume()
    {
        m_task.continuation().store(nullptr);
        onResume();
        m_task.value();
    }

private:
    Task<void, TPK>& m_task;
};

}

#endif // LEVELZ_TASK_AWAITER_HPP
