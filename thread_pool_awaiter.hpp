//
// Created by irantha on 5/16/23.
//

#ifndef LEVELZ_THREAD_POOL_AWAITER_HPP
#define LEVELZ_THREAD_POOL_AWAITER_HPP

#include <coroutine>

#include "awaiter.hpp"
#include "coroutine.hpp"
#include "thread_pool.hpp"

namespace Levelz::Async {

struct ThreadPoolAwaiter : Awaiter {
    explicit ThreadPoolAwaiter(Coroutine& coroutine) noexcept
        : Awaiter { coroutine, AwaiterKind::ThreadPool }
    {
    }

    bool await_ready() noexcept
    {
        auto suspensionAdvice = Awaiter::onReady();
        if (suspensionAdvice == Awaiter::SuspensionAdvice::shouldNotSuspend)
            return true;
        return false;
    }

    bool await_suspend(std::coroutine_handle<> awaitingCoroutine) noexcept
    {
        auto suspensionAdvice = Awaiter::onSuspend(awaitingCoroutine);
        if (suspensionAdvice == Awaiter::SuspensionAdvice::shouldNotSuspend)
            return false;

        coroutine().schedule();
        return true;
    }

    void await_resume()
    {
        Awaiter::onResume();
    }
};

}

#endif // LEVELZ_THREAD_POOL_AWAITER_HPP
