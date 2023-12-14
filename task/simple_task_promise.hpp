//
// Created by irantha on 11/23/23.
//

#ifndef LEVELZ_SIMPLE_TASK_PROMISE_HPP
#define LEVELZ_SIMPLE_TASK_PROMISE_HPP

#include <coroutine>

#include "coroutine.hpp"
#include "thread_pool_kind.hpp"

namespace Levelz::Async {

template <ThreadPoolKind>
struct SimpleTask;

template <ThreadPoolKind TPK = ThreadPoolKind::Current>
struct SimpleTaskPromise {
    SimpleTaskPromise();
    SimpleTaskPromise(const SimpleTaskPromise&) = delete;
    SimpleTaskPromise(SimpleTaskPromise&&) = delete;
    SimpleTaskPromise& operator=(const SimpleTaskPromise&) = delete;
    SimpleTaskPromise& operator=(SimpleTaskPromise&&) = delete;
    Coroutine& coroutine() noexcept;
    std::suspend_always initial_suspend() noexcept;
    std::suspend_never final_suspend() noexcept;
    void unhandled_exception();
    void return_void() noexcept;
    SimpleTask<TPK> get_return_object() noexcept;

private:
    Coroutine m_coroutine;
};

template <ThreadPoolKind TPK>
SimpleTaskPromise<TPK>::SimpleTaskPromise()
    : m_coroutine { std::coroutine_handle<SimpleTaskPromise>::from_promise(*this),
        false, TaskKind::Simple, TPK }
{
}

template <ThreadPoolKind TPK>
Coroutine& SimpleTaskPromise<TPK>::coroutine() noexcept
{
    return m_coroutine;
}

template <ThreadPoolKind TPK>
std::suspend_always SimpleTaskPromise<TPK>::initial_suspend() noexcept
{
    struct InitialSuspendAwaiter : std::suspend_always {
        void await_resume() const noexcept
        {
            assert(TPK == Coroutine::currentThreadPoolKind());
        }
    };

    return InitialSuspendAwaiter {};
}

template <ThreadPoolKind TPK>
std::suspend_never SimpleTaskPromise<TPK>::final_suspend() noexcept
{
    return {};
}

template <ThreadPoolKind TPK>
void SimpleTaskPromise<TPK>::unhandled_exception()
{
    assert(false);
}

template <ThreadPoolKind TPK>
void SimpleTaskPromise<TPK>::return_void() noexcept
{
}

template <ThreadPoolKind TPK>
SimpleTask<TPK> SimpleTaskPromise<TPK>::get_return_object() noexcept
{
    return SimpleTask { std::coroutine_handle<SimpleTaskPromise>::from_promise(*this) };
}

}

#endif // LEVELZ_SIMPLE_TASK_PROMISE_HPP
