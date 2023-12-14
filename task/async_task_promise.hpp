//
// Created by irantha on 6/3/23.
//

#ifndef LEVELZ_ASYNC_TASK_PROMISE_HPP
#define LEVELZ_ASYNC_TASK_PROMISE_HPP

#include <coroutine>
#include <stdexcept>
#include <type_traits>

#include "event/async_barrier.hpp"
#include "event/async_mutex.hpp"
#include "event/async_value.hpp"
#include "thread_pool.hpp"
#include "thread_pool_awaiter.hpp"
#include "base_promise.hpp"
#include "cancellation_error.hpp"
#include "simple_task.hpp"

namespace Levelz::Async {

template <typename ValueType, ThreadPoolKind TPK>
struct AsyncTaskPromiseBase : BasePromise {
    using CanDestroyNotStarted = std::false_type;

    explicit AsyncTaskPromiseBase(std::coroutine_handle<> handle) noexcept
        : BasePromise { handle, TaskKind::Async, TPK }
    {
    }

    auto initial_suspend() noexcept
    {
        return ThreadPoolAwaiter { coroutine() };
    }

    struct AsyncTaskFinalSuspend : Awaiter {
        explicit AsyncTaskFinalSuspend(Coroutine& coroutine) noexcept
            : Awaiter { coroutine, AwaiterKind::Final }
        {
        }

        bool await_ready() noexcept
        {
            if (coroutine().completionEvent().count() == 1) {
                coroutine().setStatus(CoroutineStatus::Completed);
                coroutine().completionEvent().countDown();
                coroutine().signalOwner();
            } else {
                BasePromise::enqueueAsyncSetCompletedStateTask(coroutine());
                coroutine().completionEvent().countDown();
            }

            auto suspensionAdvice = Awaiter::onReady();
            if (suspensionAdvice == Awaiter::SuspensionAdvice::shouldNotSuspend)
                return true;
            return false;
        }

        void await_suspend(std::coroutine_handle<> handle) noexcept
        {
            auto suspensionAdvice = Awaiter::onSuspend(handle);
            if (suspensionAdvice == Awaiter::SuspensionAdvice::shouldNotSuspend)
                handle.destroy();
        }

        void await_resume() noexcept
        {
        }
    };

    AsyncTaskFinalSuspend final_suspend() noexcept
    {
        return AsyncTaskFinalSuspend { coroutine() };
    }

    AsyncValue<ValueType>& asyncValue()
    {
        return m_asyncValue;
    }

    void unhandled_exception() noexcept
    {
        assert(Coroutine::currentCoroutine());
        m_asyncValue.setException(std::current_exception());
        setStatus(CoroutineStatus::Returned);
        m_asyncValue.signal();
    }

    void setContinuation(Coroutine*) noexcept
    {
        assert(false);
    }

protected:
    AsyncValue<ValueType> m_asyncValue;
};

template <typename ValueType, ThreadPoolKind TPK>
struct AsyncTaskPromise : public AsyncTaskPromiseBase<ValueType, TPK> {
    using promise_type = AsyncTaskPromise<ValueType, TPK>;

    AsyncTaskPromise() noexcept
        : AsyncTaskPromiseBase<ValueType, TPK> {
            std::coroutine_handle<promise_type>::from_promise(*this)
        }
    {
    }

    AsyncTask<ValueType, TPK> get_return_object() noexcept
    {
        return AsyncTask<ValueType, TPK> { std::coroutine_handle<promise_type>::from_promise(*this) };
    }

    void return_value(ValueType value) noexcept
    {
        assert(Coroutine::currentCoroutine());
        this->m_asyncValue.set(value);
        this->setStatus(CoroutineStatus::Returned);
        this->m_asyncValue.signal();
    }

    void clearValue() noexcept
    {
        this->m_asyncValue.clear();
    }
};

template <ThreadPoolKind TPK>
struct AsyncTaskPromise<void, TPK> : public AsyncTaskPromiseBase<void, TPK> {
    using promise_type = AsyncTaskPromise<void, TPK>;

    AsyncTaskPromise() noexcept
        : AsyncTaskPromiseBase<void, TPK> {
            std::coroutine_handle<AsyncTaskPromise<void, TPK>>::from_promise(*this)
        }
    {
    }

    AsyncTask<void, TPK> get_return_object() noexcept
    {
        return AsyncTask<void, TPK> { std::coroutine_handle<promise_type>::from_promise(*this) };
    }

    void return_void() noexcept
    {
        assert(Coroutine::currentCoroutine());
        this->m_asyncValue.set();
        this->setStatus(CoroutineStatus::Returned);
        this->m_asyncValue.signal();
    }

    void clearValue() noexcept
    {
        this->m_asyncValue.clear();
    }
};

}

#endif // LEVELZ_ASYNC_TASK_PROMISE_HPP
