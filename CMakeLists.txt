cmake_minimum_required(VERSION 3.9)

set(CMAKE_OSX_SYSROOT "/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk")

project(levelz VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_OSX_ARCHITECTURES "arm64")

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")

include_directories(
        ${PROJECT_SOURCE_DIR}
        "."
)
find_package(Catch2 3 REQUIRED)

add_library(levelz-async
        thread_pool.hpp
        thread_pool.cpp
        spin_wait.hpp
        event/sync_manual_reset_event.cpp
        event/sync_manual_reset_event.hpp
        event/sync_auto_reset_event.hpp
        event/sync_auto_reset_event.cpp
        thread_state.hpp
        task/task.hpp
        task/task_promise.hpp
        task/task_awaiter.hpp
        task/promise_error.hpp
        event/async_event.hpp
        task/sync_task_promise.hpp
        task/sync_task.hpp
        thread_pool_awaiter.hpp
        event/sync_auto_reset_countdown_event.hpp
        event/sync_auto_reset_countdown_event.cpp
        fifo_wait_list.hpp
        task/async_task.hpp
        task/async_task_promise.hpp
        event/async_value.hpp
        event/async_mutex.hpp
        event/async_barrier.hpp
        coroutine.hpp
        coroutine.cpp
        awaiter.cpp
        awaiter.hpp
        thread_state.cpp
        task/base_promise.hpp
        task/cancellation_error.hpp
        task/base_task.hpp
        event/async_scope.hpp
        event/async_mutex.cpp
        event/async_event.cpp
        event/async_barrier.cpp
        concurrent_fifo_list.hpp
        async_spin_wait.hpp
        async_spin_wait.cpp
        spin_wait.cpp
        event/async_scope.cpp
        task/simple_task_promise.hpp
        task/simple_task.hpp
        awaiter_kind.hpp
        task_kind.hpp
        thread_pool_kind.hpp
        event/async_countdown_event.hpp
        event/async_countdown_event.cpp
)

set_target_properties(levelz-async
        PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})

add_executable(levelz-async-tests
        # async files
        thread_pool_tests.cpp
        task/task_tests.cpp
        task/task_perf_tests.cpp
        task/generator_tests.cpp
        test/counted.hpp
        test/counted.hpp
        test/counted.cpp
        event/async_event_tests.cpp
        event/async_value_tests.cpp
        event/async_mutex_tests.cpp
        event/async_barrier_tests.cpp
        task/cancellation_tests.cpp
        test/async_test_utils.hpp
)

target_link_libraries(levelz-async-tests levelz-async Catch2::Catch2WithMain)
