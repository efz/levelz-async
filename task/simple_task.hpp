//
// Created by irantha on 11/23/23.
//

#ifndef LEVELZ_SIMPLE_TASK_HPP
#define LEVELZ_SIMPLE_TASK_HPP

#include "task_kind.hpp"
#include "simple_task_promise.hpp"

namespace Levelz::Async {

template <ThreadPoolKind TPK = ThreadPoolKind::Current>
struct [[nodiscard]] SimpleTask {
    using promise_type = SimpleTaskPromise<TPK>;

private:
    friend struct BasePromise;
    friend struct SyncAutoResetEvent;
    template <typename, ThreadPoolKind>
    friend struct SyncTaskPromiseBase;
    friend struct AsyncEvent;
    friend struct AsyncCountDownEvent;
    template <ThreadPoolKind>
    friend struct SimpleTaskPromise;

    explicit SimpleTask(std::coroutine_handle<promise_type> handle) noexcept;
    Coroutine& coroutine() noexcept;
    void clear() noexcept;

    std::coroutine_handle<promise_type> m_handle;
};

template <ThreadPoolKind TPK>
SimpleTask<TPK>::SimpleTask(std::coroutine_handle<promise_type> handle) noexcept
    : m_handle { handle }
{
}

template <ThreadPoolKind TPK>
Coroutine& SimpleTask<TPK>::coroutine() noexcept
{
    return m_handle.promise().coroutine();
}

template <ThreadPoolKind TPK>
void SimpleTask<TPK>::clear() noexcept
{
    m_handle = nullptr;
}

using DefaultSimpleTask = SimpleTask<ThreadPoolKind::Default>;
using BackgroundSimpleTask = SimpleTask<ThreadPoolKind::Background>;

}

#endif // LEVELZ_SIMPLE_TASK_HPP
