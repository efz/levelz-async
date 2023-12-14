//
// Created by irantha on 6/3/23.
//

#ifndef LEVELZ_ASYNC_TASK_HPP
#define LEVELZ_ASYNC_TASK_HPP

#include <coroutine>

#include "event/async_value.hpp"
#include "async_task_promise.hpp"
#include "base_promise.hpp"
#include "base_task.hpp"
#include "event/async_value.hpp"

namespace Levelz::Async {

template <typename ValueType, ThreadPoolKind TPK>
struct [[nodiscard]] AsyncTask : BaseTask<AsyncTaskPromise<ValueType, TPK>> {
    using promise_type = AsyncTaskPromise<ValueType, TPK>;

    AsyncTask() noexcept
        : BaseTask<promise_type> {}
    {
    }

    explicit AsyncTask(std::coroutine_handle<promise_type> handle) noexcept
        : BaseTask<promise_type> {
            handle
        }
    {
    }

    using AwaiterType = BasePromise::AsyncTaskAwaiterType<ValueType>;

private:
    friend struct BasePromise;

    AsyncValue<ValueType>& asyncValue() const noexcept
    {
        return BaseTask<promise_type>::handle().promise().asyncValue();
    }
};

template <typename ValueType = void>
using Async = AsyncTask<ValueType, ThreadPoolKind::Current>;

template <typename ValueType = void>
using DefaultAsync = AsyncTask<ValueType, ThreadPoolKind::Default>;

template <typename ValueType = void>
using BackgroundAsync = AsyncTask<ValueType, ThreadPoolKind::Background>;

}

#endif // LEVELZ_ASYNC_TASK_HPP
