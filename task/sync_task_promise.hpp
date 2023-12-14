//
// Created by irantha on 5/14/23.
//

#ifndef LEVELZ_SYNC_TASK_PROMISE_HPP
#define LEVELZ_SYNC_TASK_PROMISE_HPP

#include <coroutine>
#include <variant>

#include "event/sync_manual_reset_event.hpp"
#include "thread_pool.hpp"
#include "thread_pool_awaiter.hpp"
#include "base_promise.hpp"
#include "simple_task.hpp"

namespace Levelz::Async {

template <typename, ThreadPoolKind>
struct SyncTask;

template <typename ValueType, ThreadPoolKind TPK>
struct SyncTaskPromiseBase : BasePromise {
    using CanDestroyNotStarted = std::false_type;

    explicit SyncTaskPromiseBase(std::coroutine_handle<> handle) noexcept
        : BasePromise { handle, TaskKind::Sync, TPK }
        , m_value {}
        , m_event {}
    {
    }

    auto initial_suspend() noexcept
    {
        return ThreadPoolAwaiter { coroutine() };
    }

    struct SyncTaskFinalAwaiter : Awaiter {
        explicit SyncTaskFinalAwaiter(SyncTaskPromiseBase<ValueType, TPK>& promiseBase) noexcept
            : Awaiter { promiseBase.coroutine(), AwaiterKind::Final }
            , m_promiseBase { promiseBase }
        {
        }

        [[nodiscard]] bool await_ready() noexcept
        {
            if (coroutine().completionEvent().count() == 1) {
                coroutine().setStatus(CoroutineStatus::Completed);
                m_promiseBase.m_event.set();
                coroutine().completionEvent().countDown();
                if (coroutine().m_owner)
                    coroutine().signalOwner();
                assert(coroutine().completionEvent().isZero());
            } else {
                assert(!coroutine().completionEvent().isZero());
                auto simpleTask = m_promiseBase.asyncSetCompletedState();
                bool enqueued = coroutine().completionEvent().enqueue(simpleTask.coroutine());
                (void)enqueued;
                assert(enqueued);
                simpleTask.clear();
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

    private:
        SyncTaskPromiseBase<ValueType, TPK>& m_promiseBase;
    };

    SyncTaskFinalAwaiter final_suspend() noexcept
    {
        return SyncTaskFinalAwaiter { *this };
    }

    void unhandled_exception() noexcept
    {
        assert(Coroutine::currentCoroutine());
        setStatus(CoroutineStatus::Returned);
        m_value = std::current_exception();
    }

    void checkResult() const
    {
        assert(isDone());

        if (std::holds_alternative<std::exception_ptr>(m_value)) {
            auto exp = std::get<std::exception_ptr>(m_value);
            if (exp)
                std::rethrow_exception(std::get<std::exception_ptr>(m_value));
        }
    }

    SimpleTask<> asyncSetCompletedState() noexcept
    {
        assert(!coroutine().m_owner);
        auto handle = coroutine().handle();
        m_event.set();
        coroutine().signalOwner();
        auto currentStatus = setStatus(CoroutineStatus::Completed);
        if (currentStatus == CoroutineStatus::AbandonedFinalSuspended)
            handle.destroy();
        co_return;
    }

    void setContinuation(Coroutine*) noexcept
    {
        assert(false);
    }

protected:
    std::variant<std::exception_ptr,
        typename std::conditional<std::is_void<ValueType>::value, std::monostate, ValueType>::type>
        m_value;
    SyncManualResetEvent m_event;
};

template <typename ValueType, ThreadPoolKind TPK>
struct SyncTaskPromise : SyncTaskPromiseBase<ValueType, TPK> {
    using promise_type = SyncTaskPromise<ValueType, TPK>;

    SyncTaskPromise() noexcept
        : SyncTaskPromiseBase<ValueType, TPK> {
            std::coroutine_handle<promise_type>::from_promise(*this)
        }
    {
    }

    SyncTask<ValueType, TPK> get_return_object() noexcept
    {
        return SyncTask<ValueType, TPK> { std::coroutine_handle<promise_type>::from_promise(*this) };
    }

    ValueType value()
    {
        assert(!Coroutine::currentCoroutine() || this->m_event.isSet());
        this->m_event.wait();
        this->checkResult();
        return std::get<ValueType>(this->m_value);
    }

    void return_value(ValueType value) noexcept
    {
        assert(Coroutine::currentCoroutine());
        this->m_value = value;
        this->setStatus(CoroutineStatus::Returned);
    }

    void clearValue() noexcept
    {
        this->m_value = {};
    }
};

template <ThreadPoolKind TPK>
struct SyncTaskPromise<void, TPK> : public SyncTaskPromiseBase<void, TPK> {
    using promise_type = SyncTaskPromise<void, TPK>;

    SyncTaskPromise() noexcept
        : SyncTaskPromiseBase<void, TPK> {
            std::coroutine_handle<promise_type>::from_promise(*this)
        }
    {
    }

    SyncTask<void, TPK> get_return_object() noexcept
    {
        return SyncTask<void, TPK> { std::coroutine_handle<promise_type>::from_promise(*this) };
    }

    void value()
    {
        this->m_event.wait();
        this->checkResult();
        (void)std::get<std::monostate>(this->m_value);
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
struct SyncTaskPromise<ValueType&, TPK> : SyncTaskPromiseBase<ValueType*, TPK> {
    using promise_type = SyncTaskPromise<ValueType&, TPK>;

    SyncTaskPromise() noexcept
        : SyncTaskPromiseBase<ValueType*, TPK> {
            std::coroutine_handle<promise_type>::from_promise(*this)
        }
    {
    }

    SyncTask<ValueType&, TPK> get_return_object() noexcept
    {
        return SyncTask<ValueType&, TPK> { std::coroutine_handle<promise_type>::from_promise(*this) };
    }

    ValueType& value()
    {
        this->m_event.wait();
        this->checkResult();
        return *std::get<ValueType*>(this->m_value);
    }

    void return_value(ValueType& value) noexcept
    {
        assert(Coroutine::currentCoroutine());
        this->m_value = &value;
        this->setStatus(CoroutineStatus::Returned);
    }

    void clearValue() noexcept
    {
        this->m_value = {};
    }
};

template <typename ValueType, ThreadPoolKind TPK>
struct SyncTaskPromise<ValueType&&, TPK> : SyncTaskPromiseBase<ValueType, TPK> {
    using promise_type = SyncTaskPromise<ValueType&&, TPK>;

    SyncTaskPromise() noexcept
        : SyncTaskPromiseBase<ValueType, TPK> {
            std::coroutine_handle<promise_type>::from_promise(*this)
        }
    {
    }

    SyncTask<ValueType&&, TPK> get_return_object() noexcept
    {
        return SyncTask<ValueType&&, TPK> { std::coroutine_handle<promise_type>::from_promise(*this) };
    }

    ValueType value()
    {
        this->m_event.wait();
        this->checkResult();
        return std::move(std::get<ValueType>(this->m_value));
    }

    void return_value(ValueType&& value) noexcept
    {
        assert(Coroutine::currentCoroutine());
        this->m_value = std::move(value);
        this->setStatus(CoroutineStatus::Returned);
    }

    void clearValue() noexcept
    {
        this->m_value = {};
    }
};

}

#endif // LEVELZ_SYNC_TASK_PROMISE_HPP
