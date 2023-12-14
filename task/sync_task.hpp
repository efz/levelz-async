//
// Created by irantha on 5/14/23.
//

#ifndef LEVELZ_SYNC_TASK_HPP
#define LEVELZ_SYNC_TASK_HPP

#include <coroutine>
#include <future>

#include "base_task.hpp"
#include "sync_task_promise.hpp"

namespace Levelz::Async {

template <typename ValueType = void, ThreadPoolKind TPK = ThreadPoolKind::Current>
struct [[nodiscard]] SyncTask : BaseTask<SyncTaskPromise<ValueType, TPK>> {
    using promise_type = SyncTaskPromise<ValueType, TPK>;

    SyncTask(const SyncTask&) = delete;
    SyncTask<ValueType>& operator=(const SyncTask&) = delete;

    SyncTask(SyncTask&& other) noexcept
        : BaseTask<promise_type> {
            std::move(other)
        }
    {
    }

    SyncTask& operator=(SyncTask&& other) noexcept
    {
        if (&other != this) {
            *this = BaseTask<promise_type>::operator=(std::move(other));
        }
        return *this;
    }

    ValueType get() const
    {
        return BaseTask<promise_type>::m_handle.promise().value();
    }

private:
    template <typename, ThreadPoolKind>
    friend struct SyncTaskPromise;

    explicit SyncTask(std::coroutine_handle<promise_type> coroutine) noexcept
        : BaseTask<promise_type> {
            coroutine
        }
    {
    }
};

template <typename ValueType, ThreadPoolKind TPK>
struct [[nodiscard]] SyncTask<ValueType&&, TPK> : BaseTask<SyncTaskPromise<ValueType&&, TPK>> {
    using promise_type = SyncTaskPromise<ValueType&&, TPK>;

    SyncTask(const SyncTask&) = delete;
    SyncTask<ValueType>& operator=(const SyncTask&) = delete;

    SyncTask(SyncTask&& other) noexcept
        : BaseTask<promise_type> {
            std::move(other)
        }
    {
    }

    SyncTask& operator=(SyncTask&& other) noexcept
    {
        if (&other != this) {
            *this = BaseTask<promise_type>::operator=(std::move(other));
        }
        return *this;
    }

    ValueType get() const
    {
        return std::move(BaseTask<promise_type>::m_handle.promise().value());
    }

private:
    template <typename, ThreadPoolKind>
    friend struct SyncTaskPromise;

    explicit SyncTask(std::coroutine_handle<promise_type> coroutine) noexcept
        : BaseTask<promise_type> {
            coroutine
        }
    {
    }
};

template <typename ValueType = void>
using Sync = SyncTask<ValueType>;

template <typename ValueType = void>
using DefaultSync = SyncTask<ValueType, ThreadPoolKind::Default>;

template <typename ValueType = void>
using BackgroundSync = SyncTask<ValueType, ThreadPoolKind::Background>;

}

#endif // LEVELZ_SYNC_TASK_HPP
