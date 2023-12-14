//
// Created by irantha on 5/6/23.
//

#ifndef LEVELZ_TASK_HPP
#define LEVELZ_TASK_HPP

#include <cassert>
#include <coroutine>

#include "coroutine.hpp"
#include "thread_pool.hpp"
#include "base_task.hpp"
#include "task_awaiter.hpp"
#include "task_promise.hpp"

namespace Levelz::Async {

template <typename ValueType = void, ThreadPoolKind TPK = ThreadPoolKind::Current>
struct [[nodiscard]] Task : BaseTask<TaskPromise<ValueType, TPK>> {
    using promise_type = TaskPromise<ValueType, TPK>;

    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    Task(Task&& t) noexcept
        : BaseTask<promise_type> {
            std::move(t)
        }
    {
    }

    Task& operator=(Task&& other) noexcept
    {
        if (&other != this) {
            *this = BaseTask<promise_type>::operator=(std::move(other));
        }
        return *this;
    }

private:
    template <typename, ThreadPoolKind>
    friend struct TaskAwaiter;
    friend struct BasePromise;
    template <typename, ThreadPoolKind>
    friend struct TaskPromise;

    using AwaiterType = TaskAwaiter<ValueType, TPK>;

    explicit Task(std::coroutine_handle<promise_type> handle) noexcept
        : BaseTask<promise_type> {
            handle
        }
    {
    }

    ValueType value() const
    {
        return BaseTask<promise_type>::m_handle.promise().value();
    }

    std::atomic<Coroutine*>& continuation() noexcept
    {
        return BaseTask<promise_type>::m_handle.promise().continuation();
    }
};

template <ThreadPoolKind TPK>
struct [[nodiscard]] Task<void, TPK> : BaseTask<TaskPromise<void, TPK>> {
    using promise_type = TaskPromise<void, TPK>;

    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    Task(Task&& t) noexcept
        : BaseTask<promise_type> {
            std::move(t)
        }
    {
    }

    Task& operator=(Task&& other) noexcept
    {
        if (&other != this) {
            *this = BaseTask<promise_type>::operator=(std::move(other));
        }
        return *this;
    }

private:
    template <typename, ThreadPoolKind>
    friend struct TaskAwaiter;
    friend struct BasePromise;
    template <typename, ThreadPoolKind>
    friend struct TaskPromise;

    using AwaiterType = TaskAwaiter<void, TPK>;

    explicit Task(std::coroutine_handle<promise_type> handle) noexcept
        : BaseTask<promise_type> {
            handle
        }
    {
    }

    void value() const
    {
        return BaseTask<promise_type>::m_handle.promise().value();
    }

    std::atomic<Coroutine*>& continuation() noexcept
    {
        return BaseTask<promise_type>::m_handle.promise().continuation();
    }
};

template <typename ValueType, ThreadPoolKind TPK>
struct [[nodiscard]] Task<ValueType&&, TPK> : BaseTask<TaskPromise<ValueType&&, TPK>> {
    using promise_type = TaskPromise<ValueType&&, TPK>;

    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    Task(Task&& t) noexcept
        : BaseTask<promise_type> {
            std::move(t)
        }
    {
    }

    Task& operator=(Task&& other) noexcept
    {
        if (&other != this) {
            *this = BaseTask<promise_type>::operator=(std::move(other));
        }
        return *this;
    }

private:
    template <typename, ThreadPoolKind>
    friend struct TaskAwaiter;
    friend struct BasePromise;
    template <typename, ThreadPoolKind>
    friend struct TaskPromise;

    using AwaiterType = TaskAwaiter<ValueType&&, TPK>;

    explicit Task(std::coroutine_handle<promise_type> handle) noexcept
        : BaseTask<promise_type> {
            handle
        }
    {
    }

    ValueType value()
    {
        return std::move(BaseTask<promise_type>::m_handle.promise().value());
    }

    std::atomic<Coroutine*>& continuation() noexcept
    {
        return BaseTask<promise_type>::m_handle.promise().continuation();
    }
};

template <typename ValueType>
using Generator = Task<ValueType, ThreadPoolKind::Default>;

template <typename ValueType = void>
using DefaultTask = Task<ValueType, ThreadPoolKind::Default>;

template <typename ValueType = void>
using BackgroundTask = Task<ValueType, ThreadPoolKind::Background>;

}

#endif // LEVELZ_TASK_HPP
