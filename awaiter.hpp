//
// Created by irantha on 6/11/23.
//

#ifndef LEVELZ_AWAITER_HPP
#define LEVELZ_AWAITER_HPP

#include <atomic>
#include <coroutine>

#include "awaiter_kind.hpp"

namespace Levelz::Async {

struct Coroutine;

struct Awaiter {
    enum class SuspensionAdvice {
        maySuspend,
        shouldSuspend,
        shouldNotSuspend
    };

    Awaiter(Coroutine& coroutine, AwaiterKind kind) noexcept;
    [[nodiscard]] Coroutine& coroutine() const noexcept;
    SuspensionAdvice onSuspend(std::coroutine_handle<> handle);
    void onResume();
    [[nodiscard]] SuspensionAdvice onReady();
    void setNext(Awaiter* next) noexcept;
    [[nodiscard]] Awaiter* next() const noexcept;
    [[nodiscard]] AwaiterKind kind() const noexcept;
    static bool cancel(Awaiter* awaiter) noexcept;
    bool maybeBlocked() const noexcept;

protected:
    void setMaybeBlocked(bool maybeBlocked) noexcept;

private:
    Coroutine& m_coroutine;
    std::atomic<Awaiter*> m_next;
    const AwaiterKind m_kind;
    std::atomic<bool> m_maybeBlocked;
};

}

#endif // LEVELZ_AWAITER_HPP
