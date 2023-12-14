//
// Created by irantha on 6/27/23.
//

#ifndef LEVELZ_CONCURRENT_FIFO_LIST_HPP
#define LEVELZ_CONCURRENT_FIFO_LIST_HPP

#include <atomic>
#include <cassert>
#include <stdexcept>

#include "spin_wait.hpp"

namespace Levelz::Async {

template <typename ListNode>
struct ConcurrentFifoList {
    ConcurrentFifoList() noexcept
        : m_head { nullptr }
        , m_tail { nullptr }
    {
    }

    ListNode* dequeue() noexcept
    {
        ListNode* oldHead = m_head;

        do {
            if (!oldHead) {
                if (!m_tail) {
                    return nullptr;
                } else {
                    SpinWait spinWait;
                    do {
                        spinWait.spinOne();
                        oldHead = m_head;
                        if (!oldHead && !m_tail)
                            return nullptr;
                    } while (!oldHead);
                }
            }
        } while (!m_head.compare_exchange_weak(oldHead, nullptr));

        assert(!m_head);
        assert(oldHead);
        auto* newHead = oldHead->next();
        m_head = newHead;
        assert(oldHead);
        if (!newHead) {
            assert(!m_head);
            auto* oldTail = oldHead;
            if (!m_tail.compare_exchange_strong(oldTail, nullptr)) {
                SpinWait spinWait;
                while (!oldHead->next()) {
                    spinWait.spinOne();
                }
                assert(!m_head);
                m_head = oldHead->next();
            }
        }
        oldHead->setNext(nullptr);
        m_count--;
        return oldHead;
    }

    void enqueue(ListNode* node) noexcept
    {
        node->setNext(nullptr);

        auto* oldTail = m_tail.exchange(node);

        if (oldTail) {
            assert(!oldTail->next());
            oldTail->setNext(node);
        } else {
            assert(!m_head);
            auto* oldHead = m_head.exchange(node);
            assert(!oldHead);
            (void)oldHead;
        }
        m_count++;
    }

    bool isEmpty() const noexcept
    {
        return m_head == nullptr;
    }

    uint64_t count() const noexcept
    {
        return m_count;
    }

    bool remove(ListNode* nodeToRemove) noexcept
    {
        uint64_t initialCount = m_count;
        for (uint64_t i = 0; i < 2 * initialCount; i++) {
            auto* node = dequeue();
            if (!node)
                return false;
            if (node == nodeToRemove)
                return true;
            enqueue(node);
        }
        return false;
    }

private:
    std::atomic<ListNode*> m_head;
    std::atomic<ListNode*> m_tail;
    std::atomic<uint64_t> m_count;
};

}

#endif // LEVELZ_CONCURRENT_FIFO_LIST_HPP
