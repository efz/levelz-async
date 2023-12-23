//
// Created by irantha on 5/3/23.
//

#include <cstdio>
#include <stdexcept>
#include <thread>

#include "coroutine.hpp"
#include "spin_wait.hpp"
#include "task/cancellation_error.hpp"
#include "thread_pool.hpp"
#include "thread_state.hpp"

namespace Levelz::Async {

namespace {
    constexpr int s_maxThreadCount = 10;
    constexpr int s_backgroundThreadCount = 5;

    ThreadPool& defaultThreadPool()
    {
        static ThreadPool s_threadPool { std::min(s_maxThreadCount, static_cast<int>(std::thread::hardware_concurrency())),
            ThreadPoolKind::Default };
        return s_threadPool;
    }

    ThreadPool& backgroundThreadPool()
    {
        static ThreadPool s_threadPool { std::min(s_backgroundThreadCount, s_maxThreadCount),
            ThreadPoolKind::Background };
        return s_threadPool;
    }
}

thread_local ThreadState* ThreadPool::s_currentState = nullptr;
thread_local ThreadPool* ThreadPool::s_currentThreadPool = nullptr;
thread_local Coroutine* ThreadPool::s_currentCoroutine = nullptr;

ThreadPool::ThreadPool(int threadCount, ThreadPoolKind kind)
    : m_threads {}
    , m_threadCount { threadCount > 0 ? std::min(s_maxThreadCount, threadCount) : 1 }
    , m_threadStates { std::make_unique<ThreadState[]>(m_threadCount) }
    , m_globalQueue {}
    , m_mayBeSleepingThreadCount { 0 }
    , m_sleepingThreadCount { 0 }
    , m_noLocalWork { kind == ThreadPoolKind::Background }
    , m_state { State::NotStarted }
    , m_kind { kind }
    , m_pendingWakeUpRequestCount { 0 }
{
    assert(kind == ThreadPoolKind::Default || kind == ThreadPoolKind::Background);

    m_threads.reserve(threadCount);
    for (int i = 0; i < m_threadCount; ++i) {
        m_threadStates[i].setThreadIndex(i);
        m_threads.emplace_back([this, i] { runWorkerThread(i); });
    }
    m_state = State::Started;
}

ThreadPool::~ThreadPool()
{
    if (m_state != State::Terminated)
        shutdown();
}

Coroutine* ThreadPool::tryGetRemote() noexcept
{
    assert(s_currentState);
    auto* coroutine = tryGlobalDequeue();
    if (!m_noLocalWork && !coroutine)
        coroutine = tryStealFromOtherThread();
    return coroutine;
}

Coroutine* ThreadPool::tryGetWork() noexcept
{
    assert(s_currentState);
    Coroutine* coroutine = nullptr;
    bool tryRemote = m_noLocalWork || s_currentState->rand() % 128 == 0;
    if (tryRemote)
        coroutine = tryGetRemote();

    if (!coroutine)
        coroutine = s_currentState->tryLocalPop();
    if (!coroutine && !tryRemote)
        coroutine = tryGetRemote();

    return coroutine;
}

void ThreadPool::runWorkerThread(int threadIndex) noexcept
{
    auto& localState = m_threadStates[threadIndex];
    s_currentState = &localState;
    s_currentThreadPool = this;
    assert(threadIndex == s_currentState->threadIndex());

    while (true) {
        Coroutine* coroutine;

        while (true) {
            if (s_currentThreadPool->m_state == State::ShuttingDownImmediately)
                return;

            coroutine = tryGetWork();
            if (!coroutine)
                break;
            processPendingWakeUps();
            resume(coroutine, ThreadState::s_maxChainedExecutionAllowance);
        }

        SpinWait spinWait;
        while (true) {
            for (int i = 0; i < s_numRemoteWorksChecksBeforeSleep; ++i) {
                coroutine = tryGetRemote();

                if (coroutine)
                    goto normal_processing;

                if (s_currentThreadPool->m_state == State::ShuttingDown
                    || s_currentThreadPool->m_state == State::ShuttingDownImmediately)
                    return;

                spinWait.spinOne();
            }

            coroutine = tryGetRemote();
            if (coroutine)
                goto normal_processing;
            if (m_pendingWakeUpRequestCount != 0)
                goto normal_processing;

            if (!localState.isSleeping()) {
                setSleeping(true);
                continue;
            } else {
                assert(m_sleepingThreadCount < m_threadCount);
                m_sleepingThreadCount++;
                localState.sleepUntilWoken();
                m_sleepingThreadCount--;
                assert(m_sleepingThreadCount < m_threadCount);
                setSleeping(false);
            }
        }

    normal_processing:
        if (localState.isSleeping())
            setSleeping(false);
        else
            processPendingWakeUps();

        if (coroutine)
            resume(coroutine, ThreadState::s_maxChainedExecutionAllowance);
    }
}

void ThreadPool::setSleeping(bool isSleeping)
{
    if (isSleeping) {
        s_currentState->setSleeping(true);
        m_mayBeSleepingThreadCount++;
    } else {
        s_currentState->setSleeping(false);
        m_mayBeSleepingThreadCount--;
        if (m_mayBeSleepingThreadCount == 0)
            m_pendingWakeUpRequestCount = 0;
    }
}

void ThreadPool::resume(Coroutine* coroutine, int chainedExecutionAllowance)
{
    assert(s_currentState);
    assert(chainedExecutionAllowance > 0);
    s_currentState->setChainedExecutionAllowance(chainedExecutionAllowance);

    try {
        coroutine->resume();
    } catch (const CancellationError& e) {
        if (isImmediateShutdownRequested())
            return;
        printf("error resuming \n");
        printf("%s", e.what());
        std::abort();
    } catch (const std::exception& e) {
        printf("error resuming \n");
        printf("%s", e.what());
        std::abort();
    } catch (...) {
        printf("error resuming \n");
        std::abort();
    }
}

void ThreadPool::shutdown()
{
    shutdown(State::ShuttingDown);
}

void ThreadPool::shutdownImmediately()
{
    shutdown(State::ShuttingDownImmediately);
}

void ThreadPool::shutdown(State state)
{
    assert(state == State::ShuttingDown || state == State::ShuttingDownImmediately);

    m_state = state;

    for (int i = 0; i < m_threads.size(); ++i) {
        auto& threadState = m_threadStates[i];
        threadState.wakeUpIfSleeping();
    }

    for (auto& t : m_threads) {
        t.join();
    }

    m_state = State::Terminated;
}

void ThreadPool::scheduleOnThreadPool(Coroutine* coroutine) noexcept
{
    assert(coroutine->threadPoolKind() == kind());
    if (ThreadPool::isShutdownRequested())
        coroutine->setCancelled();

    if (s_currentThreadPool != this || m_noLocalWork)
        globalEnqueue(coroutine);
    else
        s_currentState->localEnqueue(coroutine);

    wakeOneThread();
}

bool ThreadPool::isShutdownRequested() noexcept
{
    return s_currentThreadPool
        && (s_currentThreadPool->m_state == State::ShuttingDown
            || s_currentThreadPool->m_state == State::ShuttingDownImmediately);
}

bool ThreadPool::isImmediateShutdownRequested() noexcept
{
    return s_currentThreadPool
        && s_currentThreadPool->m_state == State::ShuttingDownImmediately;
}

void ThreadPool::globalEnqueue(Coroutine* operation) noexcept
{
    m_globalQueue.enqueue(operation);
}

Coroutine* ThreadPool::tryGlobalDequeue() noexcept
{
    return m_globalQueue.dequeue();
}

Coroutine* ThreadPool::tryStealFromOtherThread() noexcept
{
    if (m_noLocalWork)
        return nullptr;
    for (int i = 0; i < 2 * m_threadCount; i++) {
        int otherThreadIndex = static_cast<int>(s_currentState->rand()) % m_threadCount;
        auto& otherThreadState = m_threadStates[otherThreadIndex];
        auto* coroutine = otherThreadState.tryLocalPop();
        if (coroutine) {
            return coroutine;
        }
    }
    return nullptr;
}

bool ThreadPool::wakeOneThread(bool doImmediateWakeUp) noexcept
{
    for (int i = 0; i < m_threadCount; ++i) {
        if (m_mayBeSleepingThreadCount == 0)
            return true;

        auto success = doImmediateWakeUp
            ? m_threadStates[i].wakeUpIfSleeping()
            : m_threadStates[i].tryWakeUpIfSleeping();

        if (success)
            return true;
    }
    return false;
}

void ThreadPool::wakeOneThread() noexcept
{
    bool doImmediateWakeUp = !s_currentThreadPool || s_currentThreadPool->noLocalWork()
        || (s_currentThreadPool != this && m_mayBeSleepingThreadCount == m_threadCount);

    bool success = wakeOneThread(doImmediateWakeUp);
    if (success)
        return;

    if (doImmediateWakeUp)
        return;
    if (m_mayBeSleepingThreadCount == 0)
        return;
    if (s_currentThreadPool && s_currentThreadPool != this
        && m_mayBeSleepingThreadCount == m_threadCount && m_pendingWakeUpRequestCount > 0) {
        (void)wakeOneThread(true);
        return;
    }

    int pendingWakeUpRequestCount = m_pendingWakeUpRequestCount;
    do {
        assert(pendingWakeUpRequestCount >= 0);
        assert(pendingWakeUpRequestCount <= m_threadCount);
        if (pendingWakeUpRequestCount >= m_mayBeSleepingThreadCount)
            return;
    } while (!m_pendingWakeUpRequestCount.compare_exchange_weak(
        pendingWakeUpRequestCount, pendingWakeUpRequestCount + 1));
}

void ThreadPool::processPendingWakeUps() noexcept
{
    assert(m_pendingWakeUpRequestCount >= 0);
    assert(m_pendingWakeUpRequestCount <= m_threadCount);

    int pendingWakeUpRequestCount = m_pendingWakeUpRequestCount;
    do {
        assert(pendingWakeUpRequestCount >= 0);
        if (pendingWakeUpRequestCount == 0)
            return;
    } while (!m_pendingWakeUpRequestCount.compare_exchange_weak(
        pendingWakeUpRequestCount, pendingWakeUpRequestCount - 1));

    wakeOneThread();
}

bool ThreadPool::haveWork() const noexcept
{
    bool haveWork = !m_globalQueue.isEmpty();
    if (m_noLocalWork)
        return haveWork;
    for (int i = 0; i < m_threadCount; ++i) {
        haveWork = haveWork || m_threadStates[i].haveLocalWork();
    }
    return haveWork;
}

int ThreadPool::sleepingThreadCount() const noexcept
{
    return m_sleepingThreadCount;
}

void ThreadPool::waitForAllThreadsIdle()
{
    SpinWait spinWait;
    ThreadPool& dtPool = defaultThreadPool();
    while (dtPool.sleepingThreadCount() < dtPool.threadCount() || dtPool.haveWork())
        spinWait.spinOne();

    ThreadPool& bkPool = defaultThreadPool();
    while (bkPool.sleepingThreadCount() < bkPool.threadCount() || bkPool.haveWork())
        spinWait.spinOne();
}

int ThreadPool::maxThreadCount() noexcept
{
    return s_maxThreadCount;
}

int ThreadPool::threadCount() const noexcept
{
    return m_threadCount;
}

ThreadPool* ThreadPool::currentThreadPool() noexcept
{
    return s_currentThreadPool;
}

bool ThreadPool::canDoChainedExecution() noexcept
{
    return !s_currentState || s_currentState->chainedExecutionAllowance() > 0;
}

void ThreadPool::recordChainedExecution() noexcept
{
    assert(s_currentState);
    assert(s_currentState->chainedExecutionAllowance() > 0);
    s_currentState->recordChainedExecution();
}

void ThreadPool::yield()
{
    if (!s_currentState) {
        std::this_thread::yield();
        return;
    }

    auto* coroutine = s_currentThreadPool->tryGetWork();
    if (coroutine) {
        auto* currentCoroutine = ThreadPool::currentCoroutine();
        auto allowance = s_currentState->chainedExecutionAllowance();
        resume(coroutine, 1);
        s_currentState->setChainedExecutionAllowance(allowance);
        ThreadPool::setCurrentCoroutine(currentCoroutine);
    } else {
        std::this_thread::yield();
    }
}

void ThreadPool::shutdownAll()
{
    defaultThreadPool().shutdown();
    backgroundThreadPool().shutdown();
}

Coroutine* ThreadPool::currentCoroutine() noexcept
{
    return s_currentCoroutine;
}
void ThreadPool::setCurrentCoroutine(Coroutine* coroutine) noexcept
{
    s_currentCoroutine = coroutine;
}

bool ThreadPool::noLocalWork() const noexcept
{
    return m_noLocalWork;
}

ThreadPoolKind ThreadPool::kind() const noexcept
{
    return m_kind;
}

ThreadPoolKind ThreadPool::currentThreadPoolKind() noexcept
{
    if (!currentThreadPool())
        return ThreadPoolKind::Current;
    return currentThreadPool()->kind();
}

ThreadPool& ThreadPool::threadPool(ThreadPoolKind kind) noexcept
{
    if (kind == ThreadPoolKind::Background)
        return backgroundThreadPool();
    else if (kind == ThreadPoolKind::Default)
        return defaultThreadPool();
    else
        std::abort();
}

}
