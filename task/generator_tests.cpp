//
// Created by irantha on 6/17/23.
//

#include <catch2/catch_test_macros.hpp>
#include <stdexcept>
#include <vector>

#include "task/sync_task.hpp"
#include "task/task.hpp"

namespace Levelz::Async::Test {

TEST_CASE("Generator - simple", "[Generator]")
{
    auto generator = []() -> Generator<int> {
        co_yield 1;
        co_yield 2;
        co_return 3;
    };

    auto run = [&]() -> SyncTask<> {
        auto seq = generator();
        auto value1 = co_await seq;
        REQUIRE(value1 == 1);
        REQUIRE(!seq.isDone());
        auto value2 = co_await seq;
        REQUIRE(value2 == 2);
        REQUIRE(!seq.isDone());
        auto value3 = co_await seq;
        REQUIRE(value3 == 3);
        REQUIRE(seq.isDone());
        auto value4 = co_await seq; // after return
        REQUIRE(value4 == 3);
        REQUIRE(seq.isDone());
    };
    run().get();
}

TEST_CASE("Generator - sorted vector iterator", "[Generator]")
{
    auto seqGen = [](std::vector<int>& vec) -> Generator<int> {
        for (auto x : vec)
            co_yield x;
        co_return -1;
    };

    auto run = [&]() -> SyncTask<> {
        std::vector<int> vec { 2, 5, 7, 11 };
        auto gen = seqGen(vec);
        int i = 0;
        while (!gen.isDone()) {
            int x = co_await gen;
            if (gen.isDone()) {
                REQUIRE(x == -1);
                break;
            }
            REQUIRE(x == vec[i++]);
        }
        REQUIRE(i == 4);
    };

    run().get();
}

TEST_CASE("Generator - exception on start", "[Generator]")
{
    auto gen = [](bool shouldThrow) -> Generator<int> {
        if (shouldThrow)
            throw std::runtime_error("invalid");
        else
            co_yield 1;
        co_return 2;
    };

    auto run = [&]() -> SyncTask<> {
        auto seq = gen(true);
        REQUIRE_THROWS_AS(co_await seq, std::runtime_error);
    };

    run().get();
}

TEST_CASE("Generator - exception after yield", "[Generator]")
{
    auto gen = [](bool shouldThrow) -> Generator<int> {
        co_yield 1;
        if (shouldThrow)
            throw std::runtime_error("invalid");
        else
            co_yield 2;
        co_return 3;
    };

    auto run = [&]() -> SyncTask<> {
        auto seq = gen(true);
        int value = co_await seq;
        REQUIRE(value == 1);
        REQUIRE_THROWS_AS(co_await seq, std::runtime_error);
    };

    run().get();
}

TEST_CASE("Generator - ref value generator", "[Generator]")
{
    int value = 0;
    auto gen = [&]() -> Generator<int&> {
        value++;
        co_yield value;
        value++;
        co_yield value;
        value++;
        co_return value;
    };

    auto run = [&]() -> SyncTask<> {
        auto seq = gen();
        decltype(auto) val1 = co_await seq;
        REQUIRE(val1 == 1);
        REQUIRE(&val1 == &value);
        decltype(auto) val2 = co_await seq;
        REQUIRE(val2 == 2);
        REQUIRE(&val1 == &value);
        decltype(auto) val3 = co_await seq;
        REQUIRE(val3 == 3);
        REQUIRE(&val3 == &value);
        REQUIRE(seq.isDone());
    };

    run().get();
}

TEST_CASE("Generator - generator of generator", "[Generator]")
{
    auto seqGen = [](std::vector<int>& vec) -> Generator<int> {
        for (auto x : vec)
            co_yield x;
        co_return -1;
    };

    auto seqGenGen = [](std::vector<Generator<int>>& generators) -> Generator<int> {
        for (auto& gen : generators) {
            while (!gen.isDone()) {
                int x = co_await gen;
                if (gen.isDone())
                    break;
                co_yield x;
            }
        }
        co_return -1;
    };

    auto run = [&]() -> SyncTask<int> {
        std::vector<int> vec1 { 2, 5, 11 };
        std::vector<int> vec2 { 0 };
        std::vector<int> vec3 {};
        std::vector<int> vec4 { 9, 3 };

        std::vector<Generator<int>> generators;
        generators.push_back(seqGen(vec1));
        generators.push_back(seqGen(vec2));
        generators.push_back(seqGen(vec3));
        generators.push_back(seqGen(vec4));

        auto genGen = seqGenGen(generators);

        int count = 0;
        for (auto vec : { vec1, vec2, vec3, vec4 })
            for (auto x : vec) {
                auto y = co_await genGen;
                REQUIRE(x == y);
                count++;
            }
        co_return count;
    };

    REQUIRE(run().get() == 6);
}

TEST_CASE("Generator - unique_ptr value generator", "[Generator]")
{
    auto gen = [&]() -> Generator<std::unique_ptr<int>&&> {
        co_yield std::make_unique<int>(1);
        co_yield std::make_unique<int>(2);
        co_yield std::make_unique<int>(3);
        co_return std::make_unique<int>(4);
    };

    auto run = [&]() -> SyncTask<> {
        auto seq = gen();
        auto val1 = co_await seq;
        REQUIRE(*val1 == 1);
        auto val2 = co_await seq;
        REQUIRE(*val2 == 2);
        auto val3 = co_await seq;
        REQUIRE(*val3 == 3);
        auto val4 = co_await seq;
        REQUIRE(*val4 == 4);
        REQUIRE(seq.isDone());
    };
    run().get();
}

}
