//
// Created by irantha on 5/6/23.
//

#ifndef LEVELZ_TASK_PROMISE_HPP
#define LEVELZ_TASK_PROMISE_HPP

#include <cassert>
#include <coroutine>
#include <stdexcept>
#include <variant>

#include "coroutine.hpp"
#include "event/async_event.hpp"
#include "event/async_mutex.hpp"
#include "event/async_value.hpp"
#include "thread_pool.hpp"
#include "thread_pool_awaiter.hpp"
#include "thread_pool_kind.hpp"
#include "base_promise.hpp"
#include "promise_error.hpp"
#include "simple_task.hpp"

namespace Levelz::Async {

template <typename, ThreadPoolKind>
struct Task;

template <typename, ThreadPoolKind>
struct TaskAwaiter;

struct TaskPromiseYieldSuspendAwaiter : Awaiter {
    TaskPromiseYieldSuspendAwaiter(
        Coroutine& coroutine, Coroutine* continuation) noexcept
        : Awaiter { coroutine, AwaiterKind::Yield }
        , m_continuation { continuation }
    {
    }

    [[nodiscard]] bool await_ready() noexcept
    {
        auto suspensionAdvice = Awaiter::onReady();
        if (suspensionAdvice == Awaiter::SuspensionAdvice::shouldNotSuspend)
            return true;
        return false;
    }

    std::coroutine_handle<> await_suspend(std::coroutine_handle<> handle) noexcept
    {
        auto* continuation = m_continuation;
        auto suspensionAdvice = Awaiter::onSuspend(handle);
        if (suspensionAdvice == SuspensionAdvice::shouldNotSuspend)
            return handle;
        if (!continuation)
            return std::noop_coroutine();

        if (suspensionAdvice == SuspensionAdvice::shouldSuspend) {
            continuation->schedule();
            return std::noop_coroutine();
        }

        if (continuation->threadPoolKind() != ThreadPool::currentThreadPoolKind()) {
            continuation->schedule();
            return std::noop_coroutine();
        }

        return continuation->handle();
    }

    void await_resume()
    {
        Awaiter::onResume();
    }

private:
    Coroutine* m_continuation;
};

struct TaskPromiseFinalSuspendAwaiter : Awaiter {
    explicit TaskPromiseFinalSuspendAwaiter(Coroutine& coroutine, Coroutine* continuation) noexcept
        : Awaiter { coroutine, AwaiterKind::Final }
        , m_continuation { continuation }
    {
    }

    [[nodiscard]] bool await_ready() noexcept
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
        if (suspensionAdvice == Awaiter::SuspensionAdvice::shouldNotSuspend) {
            if (m_continuation)
                m_continuation->schedule();
            return true;
        }
        return false;
    }

    std::coroutine_handle<> await_suspend(std::coroutine_handle<> handle) noexcept
    {
        auto* continuation = m_continuation;
        auto suspensionAdvice = Awaiter::onSuspend(handle);
        if (suspensionAdvice == Awaiter::SuspensionAdvice::shouldNotSuspend)
            handle.destroy();

        if (!continuation)
            return std::noop_coroutine();

        if (suspensionAdvice == Awaiter::SuspensionAdvice::shouldSuspend) {
            continuation->schedule();
            return std::noop_coroutine();
        }

        if (continuation->threadPoolKind() != ThreadPool::currentThreadPoolKind()) {
            continuation->schedule();
            return std::noop_coroutine();
        }

        return continuation->handle();
    }

    void await_resume() noexcept
    {
    }

private:
    Coroutine* m_continuation;
};

template <typename ValueType, ThreadPoolKind TPK>
struct TaskPromiseBase : BasePromise {
    using CanDestroyNotStarted = std::true_type;

    explicit TaskPromiseBase(std::coroutine_handle<> handle) noexcept
        : BasePromise { handle, TaskKind::Task, TPK }
        , m_continuation { nullptr }
        , m_value {}
    {
    }

    struct InitialSuspendAwaiter : Awaiter {
        explicit InitialSuspendAwaiter(Coroutine& coroutine,
            TaskPromiseBase<ValueType, TPK>& taskPromiseBase)
            : Awaiter { coroutine, AwaiterKind::Initial }
            , m_taskPromiseBase { taskPromiseBase }
        {
        }

        [[nodiscard]] bool await_ready() noexcept
        {
            auto suspensionAdvice = Awaiter::onReady();
            (void)suspensionAdvice;
            assert(suspensionAdvice != Awaiter::SuspensionAdvice::shouldNotSuspend);
            return false;
        }

        bool await_suspend(std::coroutine_handle<> handle) noexcept
        {
            auto suspensionAdvice = Awaiter::onSuspend(handle);
            if (suspensionAdvice == Awaiter::SuspensionAdvice::shouldNotSuspend)
                return false;
            return true;
        }

        void await_resume()
        {
            Awaiter::onResume();
        }

    private:
        TaskPromiseBase<ValueType, TPK>& m_taskPromiseBase;
    };

    InitialSuspendAwaiter initial_suspend() noexcept
    {
        return InitialSuspendAwaiter { coroutine(), *this };
    }

    TaskPromiseFinalSuspendAwaiter final_suspend() noexcept
    {
        return TaskPromiseFinalSuspendAwaiter { coroutine(), m_continuation.exchange(nullptr) };
    }

    void unhandled_exception() noexcept
    {
        m_value = std::current_exception();
        setStatus(CoroutineStatus::Returned);
    }

    std::atomic<Coroutine*>& continuation() noexcept
    {
        return m_continuation;
    }

    void setContinuation(Coroutine* continuation) noexcept
    {
        m_continuation = continuation;
    }

    void checkResult() const
    {
        assert(isDone()
            || status() == CoroutineStatus::Yielded
            || status() == CoroutineStatus::YieldedSuspended);

        if (std::holds_alternative<std::exception_ptr>(m_value)) {
            auto exp = std::get<std::exception_ptr>(m_value);
            if (exp)
                std::rethrow_exception(std::get<std::exception_ptr>(m_value));
        }
    }

protected:
    std::atomic<Coroutine*> m_continuation;
    std::variant<std::exception_ptr,
        typename std::conditional<std::is_void<ValueType>::value, std::monostate, ValueType>::type>
        m_value;
};

template <typename ValueType, ThreadPoolKind TPK>
struct TaskPromise : TaskPromiseBase<ValueType, TPK> {
    using promise_type = TaskPromise<ValueType, TPK>;

    TaskPromise() noexcept
        : TaskPromiseBase<ValueType, TPK>(std::coroutine_handle<promise_type>::from_promise(*this))
    {
    }

    ValueType value() const
    {
        this->checkResult();
        return std::get<ValueType>(this->m_value);
    }

    Task<ValueType, TPK> get_return_object() noexcept
    {
        return Task<ValueType, TPK> { std::coroutine_handle<promise_type>::from_promise(*this) };
    }

    void return_value(ValueType value) noexcept
    {
        assert(Coroutine::currentCoroutine());
        this->m_value = value;
        this->setStatus(CoroutineStatus::Returned);
    }

    TaskPromiseYieldSuspendAwaiter yield_value(ValueType value)
    {
        assert(Coroutine::currentCoroutine());
        this->m_value = value;
        this->setStatus(CoroutineStatus::Yielded);
        return TaskPromiseYieldSuspendAwaiter {
            this->coroutine(),
            this->m_continuation
        };
    }

    void clearValue() noexcept
    {
        TaskPromiseBase<ValueType, TPK>::m_value = {};
    }
};

template <ThreadPoolKind TPK>
struct TaskPromise<void, TPK> : TaskPromiseBase<void, TPK> {
    using promise_type = TaskPromise<void, TPK>;

    TaskPromise() noexcept
        : TaskPromiseBase<void, TPK>(std::coroutine_handle<promise_type>::from_promise(*this))
    {
    }

    void value() const
    {
        this->checkResult();
        (void)std::get<std::monostate>(this->m_value);
    }

    Task<void, TPK> get_return_object() noexcept
    {
        return Task<void, TPK> { std::coroutine_handle<promise_type>::from_promise(*this) };
    }

    void return_void() noexcept
    {
        assert(Coroutine::currentCoroutine());
        this->m_value = std::monostate {};
        this->setStatus(CoroutineStatus::Returned);
    }

    void clearValue() noexcept
    {
    }
};

template <typename ValueType, ThreadPoolKind TPK>
struct TaskPromise<ValueType&, TPK> : TaskPromiseBase<ValueType*, TPK> {
    using promise_type = TaskPromise<ValueType&, TPK>;

    TaskPromise() noexcept
        : TaskPromiseBase<ValueType*, TPK>(std::coroutine_handle<promise_type>::from_promise(*this))
    {
    }

    ValueType& value() const
    {
        this->checkResult();
        return *std::get<ValueType*>(this->m_value);
    }

    Task<ValueType&, TPK> get_return_object() noexcept
    {
        return Task<ValueType&, TPK> { std::coroutine_handle<promise_type>::from_promise(*this) };
    }

    void return_value(ValueType& value) noexcept
    {
        assert(Coroutine::currentCoroutine());
        this->m_value = &value;
        this->setStatus(CoroutineStatus::Returned);
    }

    TaskPromiseYieldSuspendAwaiter yield_value(ValueType& value)
    {
        assert(Coroutine::currentCoroutine());
        this->m_value = &value;
        this->setStatus(CoroutineStatus::Yielded);
        return TaskPromiseYieldSuspendAwaiter {
            this->coroutine(),
            this->m_continuation
        };
    }

    void clearValue() noexcept
    {
        this->m_value = nullptr;
    }
};

template <typename ValueType, ThreadPoolKind TPK>
struct TaskPromise<ValueType&&, TPK> : TaskPromiseBase<ValueType, TPK> {
    using promise_type = TaskPromise<ValueType&&, TPK>;

    TaskPromise() noexcept
        : TaskPromiseBase<ValueType, TPK>(std::coroutine_handle<promise_type>::from_promise(*this))
    {
    }

    ValueType value()
    {
        this->checkResult();
        return std::move(std::get<ValueType>(this->m_value));
    }

    Task<ValueType&&, TPK> get_return_object() noexcept
    {
        return Task<ValueType&&, TPK> { std::coroutine_handle<promise_type>::from_promise(*this) };
    }

    void return_value(ValueType&& value) noexcept
    {
        assert(Coroutine::currentCoroutine());
        this->m_value = std::move(value);
        this->setStatus(CoroutineStatus::Returned);
    }

    TaskPromiseYieldSuspendAwaiter yield_value(ValueType&& value)
    {
        assert(Coroutine::currentCoroutine());
        this->m_value = std::move(value);
        this->setStatus(CoroutineStatus::Yielded);
        return TaskPromiseYieldSuspendAwaiter {
            this->coroutine(),
            this->m_continuation
        };
    }

    void clearValue() noexcept
    {
        this->m_value = {};
    }
};

}

#endif // LEVELZ_TASK_PROMISE_HPP
