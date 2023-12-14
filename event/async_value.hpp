//
// Created by irantha on 6/3/23.
//

#ifndef LEVELZ_ASYNC_VALUE_HPP
#define LEVELZ_ASYNC_VALUE_HPP

#include <coroutine>
#include <exception>
#include <type_traits>
#include <variant>

#include "awaiter.hpp"
#include "coroutine.hpp"
#include "task/cancellation_error.hpp"
#include "thread_pool.hpp"
#include "async_event.hpp"

namespace Levelz::Async {

struct BaseAsyncValueAwaiter : Awaiter {
    explicit BaseAsyncValueAwaiter(AsyncEvent& asyncValueEvent, Coroutine& coroutine) noexcept
        : Awaiter { coroutine, AwaiterKind::Value }
        , m_asyncValueEvent { asyncValueEvent }
    {
    }

    bool await_ready() noexcept
    {
        auto suspensionAdvice = Awaiter::onReady();
        if (suspensionAdvice == Awaiter::SuspensionAdvice::shouldNotSuspend)
            return true;
        if (suspensionAdvice == Awaiter::SuspensionAdvice::shouldSuspend)
            return false;

        return m_asyncValueEvent.isSignaled();
    }

    bool await_suspend(std::coroutine_handle<> awaitingCoroutineHandle) noexcept
    {
        auto suspensionAdvice = Awaiter::onSuspend(awaitingCoroutineHandle);
        if (suspensionAdvice == Awaiter::SuspensionAdvice::shouldNotSuspend)
            return false;

        if (m_asyncValueEvent.isSignaled()) {
            if (suspensionAdvice == Awaiter::SuspensionAdvice::maySuspend) {
                return false;
            } else {
                coroutine().schedule();
                return true;
            }
        }

        setMaybeBlocked(true);
        auto enqueued = m_asyncValueEvent.enqueue(coroutine());
        setMaybeBlocked(enqueued);

        assert(enqueued || m_asyncValueEvent.isSignaled());
        if (!enqueued && suspensionAdvice == Awaiter::SuspensionAdvice::shouldSuspend) {
            coroutine().schedule();
            return true;
        }
        return enqueued;
    }

    bool cancel() noexcept
    {
        auto& coroutine = Awaiter::coroutine();
        assert(coroutine.isCancelled());
        assert(coroutine.status() == CoroutineStatus::PauseOnRunning || coroutine.status() == CoroutineStatus::Paused);
        auto removed = m_asyncValueEvent.remove(&coroutine);
        if (removed)
            coroutine.schedule();
        return removed;
    }

protected:
    AsyncEvent& m_asyncValueEvent;
};

template <typename ValueType = void>
struct BaseAsyncValue {
    BaseAsyncValue() noexcept
        : m_event {}
        , m_value { nullptr }
    {
    }

    void setExceptionAndSignal(std::exception_ptr exception) noexcept
    {
        setException(exception);
        signal();
    }

    bool isWaitListEmpty() const noexcept
    {
        return m_event.isWaitListEmpty();
    }

    bool isSignaled() const noexcept
    {
        auto signaled = m_event.isSignaled();
        assert(!signaled || !(std::holds_alternative<std::exception_ptr>(m_value) && !std::get<std::exception_ptr>(m_value)));
        return signaled;
    }

    void signal() noexcept
    {
        m_event.signal();
    }

    void enqueue(SyncManualResetEvent& syncManualResetEvent)
    {
        m_event.enqueue(syncManualResetEvent);
    }

    void setException(const std::exception_ptr& exception) noexcept
    {
        assert(exception);
        m_value = exception;
    }

    uint64_t waitListedCount() const noexcept
    {
        return m_event.waitListedCount();
    }

protected:
    AsyncEvent& event() noexcept
    {
        return m_event;
    }

    bool enqueue(Coroutine& coroutine)
    {
        return m_event.enqueue(coroutine);
    }

    void checkException() const
    {
        if (std::holds_alternative<std::exception_ptr>(m_value)) {
            auto exp = std::get<std::exception_ptr>(m_value);
            if (exp)
                std::rethrow_exception(exp);
        }
    }

    std::variant<std::exception_ptr,
        typename std::conditional<std::is_void<ValueType>::value, std::monostate, ValueType>::type>
        m_value;

private:
    AsyncEvent m_event;
};

template <typename ValueType = void>
struct AsyncValue : BaseAsyncValue<ValueType> {
    AsyncValue() noexcept
        : BaseAsyncValue<ValueType> {}
    {
    }

    void setAndSignal(ValueType value) noexcept
    {
        set(value);
        BaseAsyncValue<ValueType>::signal();
    }

    struct AsyncValueAwaiter : BaseAsyncValueAwaiter {
    public:
        explicit AsyncValueAwaiter(AsyncValue<ValueType>& asyncValue, Coroutine& coroutine) noexcept
            : BaseAsyncValueAwaiter { asyncValue.event(), coroutine }
            , m_asyncValue { asyncValue }
        {
        }

        ValueType await_resume()
        {
            Awaiter::onResume();
            return m_asyncValue.get();
        }

    private:
        AsyncValue<ValueType>& m_asyncValue;
    };

    void set(ValueType value) noexcept
    {
        BaseAsyncValue<ValueType>::m_value = value;
    }

    ValueType get()
    {
        BaseAsyncValue<ValueType>::checkException();
        return std::get<ValueType>(BaseAsyncValue<ValueType>::m_value);
    }

    void clear() noexcept
    {
        if (std::holds_alternative<ValueType>(BaseAsyncValue<ValueType>::m_value))
            BaseAsyncValue<ValueType>::m_value = {};
    }

    using AwaiterType = AsyncValueAwaiter;
};

template <>
struct AsyncValue<void> : BaseAsyncValue<void> {
    AsyncValue() noexcept
        : BaseAsyncValue {}
    {
    }

    void setAndSignal() noexcept
    {
        set();
        signal();
    }

    struct AsyncValueAwaiter : BaseAsyncValueAwaiter {
    public:
        explicit AsyncValueAwaiter(AsyncValue<void>& asyncValue, Coroutine& coroutine) noexcept
            : BaseAsyncValueAwaiter { asyncValue.event(), coroutine }
            , m_asyncValue { asyncValue }
        {
        }

        void await_resume()
        {
            Awaiter::onResume();
            m_asyncValue.get();
        }

    private:
        AsyncValue<void>& m_asyncValue;
    };

    void set() noexcept
    {
        BaseAsyncValue<void>::m_value = std::monostate {};
    }

    void get()
    {
        BaseAsyncValue<void>::checkException();
        assert(std::holds_alternative<std::monostate>(BaseAsyncValue<void>::m_value));
    }

    void clear() noexcept
    {
    }

    using AwaiterType = AsyncValueAwaiter;
};

template <typename ValueType>
struct AsyncValue<ValueType&> : BaseAsyncValue<ValueType*> {
    AsyncValue() noexcept
        : BaseAsyncValue<ValueType*> {}
    {
    }

    void setAndSignal(ValueType& value) noexcept
    {
        set(value);
        BaseAsyncValue<ValueType*>::signal();
    }

    struct AsyncValueAwaiter : BaseAsyncValueAwaiter {
    public:
        explicit AsyncValueAwaiter(AsyncValue<ValueType&>& asyncValue, Coroutine& coroutine) noexcept
            : BaseAsyncValueAwaiter {
                asyncValue.event(),
                coroutine
            }
            , m_asyncValue { asyncValue }
        {
        }

        ValueType& await_resume()
        {
            Awaiter::onResume();
            return m_asyncValue.get();
        }

    private:
        AsyncValue<ValueType&>& m_asyncValue;
    };

    void set(ValueType& value) noexcept
    {
        BaseAsyncValue<ValueType*>::m_value = &value;
    }

    ValueType& get()
    {
        BaseAsyncValue<ValueType*>::checkException();
        return *std::get<ValueType*>(BaseAsyncValue<ValueType*>::m_value);
    }

    void clear() noexcept
    {
    }

    using AwaiterType = AsyncValueAwaiter;
};

}

#endif // LEVELZ_ASYNC_VALUE_HPP
