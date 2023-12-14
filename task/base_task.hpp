//
// Created by irantha on 6/19/23.
//

#ifndef LEVELZ_BASE_TASK_HPP
#define LEVELZ_BASE_TASK_HPP

#include <coroutine>

#include "coroutine.hpp"
#include "event/async_event.hpp"

namespace Levelz::Async {

template <typename PromiseType>
struct [[nodiscard]] BaseTask {
    using promise_type = PromiseType;

    BaseTask() noexcept
        : m_handle { nullptr }
    {
    }

    explicit BaseTask(std::coroutine_handle<promise_type> handle) noexcept
        : m_handle { handle }
    {
        auto prevRefCount = m_handle.promise().ref();
        (void)prevRefCount;
        assert(prevRefCount == 0);

        if (coroutine().shouldCancelAbandoned())
            coroutine().setOwner();
    }

    ~BaseTask()
    {
        destroyOrAbandon();
    }

    BaseTask(const BaseTask& other)
        : m_handle { other.m_handle }
    {
        assert(m_handle);
        auto prevRefCount = m_handle.promise().ref();
        (void)prevRefCount;
        assert(prevRefCount > 0);

        if (coroutine().shouldCancelAbandoned())
            coroutine().setOwner();
    }

    BaseTask& operator=(const BaseTask& other)
    {
        if (&other != this) {
            destroyOrAbandon();

            m_handle = other.m_handle;
            auto prevRefCount = m_handle.promise().ref();
            (void)prevRefCount;
            assert(prevRefCount > 0);

            if (coroutine().shouldCancelAbandoned())
                coroutine().setOwner();
        }

        return *this;
    }

    BaseTask(BaseTask&& other) noexcept
        : m_handle { other.m_handle }
    {
        other.m_handle = nullptr;

        if (coroutine().shouldCancelAbandoned())
            coroutine().setOwner();
    }

    BaseTask& operator=(BaseTask&& other) noexcept
    {
        if (&other != this) {
            destroyOrAbandon();

            m_handle = other.m_handle;
            other.m_handle = nullptr;

            if (coroutine().shouldCancelAbandoned())
                coroutine().setOwner();
        }

        return *this;
    }

    [[nodiscard]] CoroutineStatus status() const noexcept
    {
        return m_handle.promise().status();
    }

    [[nodiscard]] bool isDone() const noexcept
    {
        return m_handle.promise().isDone();
    }

    [[nodiscard]] bool isCancelled() const noexcept
    {
        return m_handle.promise().isCancelled();
    }

    void cancel() noexcept
    {
        m_handle.promise().cancel();
    }

    [[nodiscard]] bool shouldCancelAbandoned() const noexcept
    {
        return m_handle.promise().shouldCancelAbandoned();
    }

    void setCancelAbandoned(bool cancelAbandoned) noexcept
    {
        m_handle.promise().setCancelAbandoned(cancelAbandoned);
    }

protected:
    Coroutine& coroutine() noexcept
    {
        return m_handle.promise().coroutine();
    }

    std::coroutine_handle<promise_type> handle() const noexcept
    {
        return m_handle;
    }

    CoroutineStatus setStatus(CoroutineStatus status) noexcept
    {
        return m_handle.promise().setStatus(status);
    }

    std::coroutine_handle<promise_type> m_handle;

private:
    void destroyOrAbandon()
    {
        if (!m_handle)
            return;

        if (shouldCancelAbandoned())
            coroutine().setOwner();

        auto prevRefCount = m_handle.promise().unRef();
        assert(prevRefCount > 0);
        if (prevRefCount != 1)
            return;

        auto currentStatus = status();
        if (currentStatus == CoroutineStatus::CompletedFinalSuspended) {
            m_handle.destroy();
            return;
        }

        if (shouldCancelAbandoned()) {
            cancel();
            auto status = coroutine().status();
            if (status == CoroutineStatus::Returned
                || status == CoroutineStatus::ReturnedCompleted
                || status == CoroutineStatus::FinalSuspended
                || status == CoroutineStatus::CompletedFinalSuspended
                || status == CoroutineStatus::Yielded
                || status == CoroutineStatus::YieldedSuspended) {
                // Not effective for AsyncTask or SyncTasks
                m_handle.promise().clearValue();
            }
            assert(status != CoroutineStatus::PauseOnRunning || status != CoroutineStatus::Paused);
        }

        auto lastStatus = setStatus(CoroutineStatus::Abandoned);
        assert(lastStatus != CoroutineStatus::Yielded
            || lastStatus != CoroutineStatus::Running);
        if (lastStatus == CoroutineStatus::CompletedFinalSuspended) {
            m_handle.destroy();
        } else if (promise_type::CanDestroyNotStarted::value
            && lastStatus == CoroutineStatus::NotStarted) {
            assert(coroutine().completionEvent().count() == 1);
            coroutine().completionEvent().countDown();
            coroutine().signalOwner();
            m_handle.destroy();
        } else if (lastStatus == CoroutineStatus::YieldedSuspended) {
            if (coroutine().completionEvent().count() == 1) {
                coroutine().completionEvent().countDown();
                coroutine().signalOwner();
                m_handle.destroy();
            } else {
                cancel();
                m_handle.promise().setContinuation(nullptr);
                coroutine().schedule();
            }
        }
    }
};

}

#endif // LEVELZ_BASE_TASK_HPP
