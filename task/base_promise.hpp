//
// Created by irantha on 6/13/23.
//

#ifndef LEVELZ_BASE_PROMISE_HPP
#define LEVELZ_BASE_PROMISE_HPP

#include <coroutine>

#include "coroutine.hpp"
#include "event/async_barrier.hpp"
#include "event/async_countdown_event.hpp"
#include "event/async_event.hpp"
#include "event/async_mutex.hpp"
#include "event/async_value.hpp"
#include "task_kind.hpp"
#include "simple_task.hpp"
#include "task_awaiter.hpp"

namespace Levelz::Async {

template <typename, ThreadPoolKind>
struct AsyncTask;

template <typename, ThreadPoolKind>
struct Task;

struct BasePromise {
    template <typename ValueType>
    using AsyncTaskAwaiterType = AsyncValue<ValueType>::AwaiterType;

    BasePromise(std::coroutine_handle<> handle, TaskKind taskKind, ThreadPoolKind threadPoolKind) noexcept
        : m_coroutine { handle, true, taskKind, threadPoolKind }
    {
    }

    BasePromise(const BasePromise&) = delete;
    BasePromise(BasePromise&&) = delete;
    BasePromise& operator=(const BasePromise&) = delete;
    BasePromise& operator=(BasePromise&&) = delete;

    template <typename ValueType, ThreadPoolKind TPK>
    inline typename AsyncTask<ValueType, TPK>::AwaiterType await_transform(AsyncTask<ValueType, TPK>& asyncTask)
    {
        if (m_coroutine.isCancelled())
            throw CancellationError {};

        return typename AsyncValue<ValueType>::AwaiterType { asyncTask.asyncValue(), m_coroutine };
    }

    template <typename ValueType, ThreadPoolKind TPK>
    inline typename AsyncTask<ValueType, TPK>::AwaiterType await_transform(AsyncTask<ValueType, TPK>&& asyncTask)
    {
        if (m_coroutine.isCancelled())
            throw CancellationError {};

        return typename AsyncValue<ValueType>::AwaiterType { asyncTask.asyncValue(), m_coroutine };
    }

    AsyncMutex::AwaiterType await_transform(AsyncMutex& mutex)
    {
        if (m_coroutine.isCancelled())
            throw CancellationError {};

        return { mutex, m_coroutine };
    }

    AsyncBarrier::AwaiterType await_transform(AsyncBarrier& barrier)
    {
        if (m_coroutine.isCancelled())
            throw CancellationError {};

        return { barrier, m_coroutine };
    }

    AsyncEvent::AwaiterType await_transform(AsyncEvent& event)
    {
        if (m_coroutine.isCancelled())
            throw CancellationError {};

        return AsyncCountDownEvent::AsyncCountDownEventAwaiter { event.m_countDownEvent, m_coroutine };
    }

    AsyncEvent::AwaiterType await_transform(AsyncCountDownEvent& event)
    {
        if (m_coroutine.isCancelled())
            throw CancellationError {};

        return AsyncCountDownEvent::AsyncCountDownEventAwaiter { event, m_coroutine };
    }

    template <typename T>
    typename AsyncValue<T>::AwaiterType await_transform(AsyncValue<T>& asyncValue)
    {
        if (m_coroutine.isCancelled())
            throw CancellationError {};

        return typename AsyncValue<T>::AsyncValueAwaiter { asyncValue, m_coroutine };
    }

    template <typename ValueType, ThreadPoolKind TPK>
    typename Task<ValueType, TPK>::AwaiterType await_transform(Task<ValueType, TPK>& task)
    {
        if (m_coroutine.isCancelled())
            throw CancellationError {};

        assert(task.status() == CoroutineStatus::YieldedSuspended
            || task.status() == CoroutineStatus::NotStarted || task.isDone());

        return TaskAwaiter<ValueType, TPK> { task, task.continuation(), coroutine(), task.coroutine() };
    }

    template <typename ValueType, ThreadPoolKind TPK>
    typename Task<ValueType, TPK>::AwaiterType await_transform(Task<ValueType, TPK>&& task)
    {
        if (m_coroutine.isCancelled())
            throw CancellationError {};

        assert(task.status() == CoroutineStatus::YieldedSuspended
            || task.status() == CoroutineStatus::NotStarted);

        return TaskAwaiter<ValueType, TPK> { task, task.continuation(), coroutine(), task.coroutine() };
    }

    Coroutine& coroutine() noexcept
    {
        return m_coroutine;
    }

    CoroutineStatus setStatus(CoroutineStatus status) noexcept
    {
        return m_coroutine.setStatus(status);
    }

    CoroutineStatus status() const noexcept
    {
        return m_coroutine.status();
    }

    bool isDone() const noexcept
    {
        return m_coroutine.isDone();
    }

    bool isCancelled() const noexcept
    {
        return m_coroutine.isCancelled();
    }

    void cancel() noexcept
    {
        m_coroutine.cancel();
    }

    bool shouldCancelAbandoned() const noexcept
    {
        return m_coroutine.shouldCancelAbandoned();
    }

    void setCancelAbandoned(bool cancelAbandoned) noexcept
    {
        m_coroutine.setCancelAbandoned(cancelAbandoned);
    }

    uint64_t ref() noexcept
    {
        assert(m_refCount >= 0);
        return m_refCount.fetch_add(1);
    }

    uint64_t unRef() noexcept
    {
        assert(m_refCount > 0);
        return m_refCount.fetch_sub(1);
    }

    uint64_t refCount() const noexcept
    {
        return m_refCount;
    }

    static SimpleTask<> asyncSetCompletedState(Coroutine& coroutine) noexcept
    {
        auto handle = coroutine.handle();
        coroutine.signalOwner();
        auto currentStatus = coroutine.setStatus(CoroutineStatus::Completed);
        if (currentStatus == CoroutineStatus::AbandonedFinalSuspended)
            handle.destroy();
        co_return;
    }

    static void enqueueAsyncSetCompletedStateTask(Coroutine& coroutine) noexcept
    {
        assert(!coroutine.completionEvent().isZero());
        auto task = asyncSetCompletedState(coroutine);
        bool enqueued = coroutine.completionEvent().enqueue(task.coroutine());
        (void)enqueued;
        assert(enqueued);
        task.clear();
    }

private:
    Coroutine m_coroutine;
    std::atomic<uint64_t> m_refCount;
};

}

#endif // LEVELZ_BASE_PROMISE_HPP
