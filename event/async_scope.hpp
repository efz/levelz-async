//
// Created by irantha on 6/19/23.
//

#ifndef LEVELZ_ASYNC_SCOPE_HPP
#define LEVELZ_ASYNC_SCOPE_HPP

#include <atomic>
#include <cinttypes>

namespace Levelz::Async {

struct AsyncScope {
    AsyncScope() noexcept;
    ~AsyncScope();

    struct [[nodiscard]] AsyncScopeTracker {
        AsyncScopeTracker()
            : m_asyncScope { nullptr }
            , m_entryCount { 0 }
        {
        }

        AsyncScopeTracker(AsyncScope* asyncScope, uint8_t entryCount)
            : m_asyncScope { asyncScope }
            , m_entryCount { entryCount }
        {
        }

        AsyncScopeTracker(const AsyncScopeTracker&) = delete;
        AsyncScopeTracker& operator=(const AsyncScopeTracker&) = delete;

        AsyncScopeTracker(AsyncScopeTracker&& other) noexcept
            : m_asyncScope { other.m_asyncScope }
            , m_entryCount { other.m_entryCount }
        {
            other.m_asyncScope = nullptr;
            other.m_entryCount = 0;
        }

        AsyncScopeTracker& operator=(AsyncScopeTracker&& other) noexcept
        {
            if (this != &other) {
                m_asyncScope = other.m_asyncScope;
                m_entryCount = other.m_entryCount;
                other.m_asyncScope = nullptr;
                other.m_entryCount = 0;
            }
            return *this;
        }

        ~AsyncScopeTracker()
        {
            if (m_asyncScope)
                m_asyncScope->onExit(m_entryCount);
        }

    private:
        AsyncScope* m_asyncScope;
        uint8_t m_entryCount;
    };

    AsyncScopeTracker onEnter() noexcept;
    bool isEmpty() const noexcept;
    void waitTillEmpty() const noexcept;

private:
    void onExit(uint8_t entryCount) noexcept;

    std::atomic<uint8_t> m_count;
};

}

#endif // LEVELZ_ASYNC_SCOPE_HPP
