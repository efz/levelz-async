//
// Created by irantha on 5/7/23.
//

#ifndef LEVELZ_PROMISE_ERROR_HPP
#define LEVELZ_PROMISE_ERROR_HPP

#include <stdexcept>

namespace Levelz::Async {

struct PromiseError : std::logic_error {
    PromiseError()
        : std::logic_error("promise error")
    {
    }

    explicit PromiseError(const std::string& description)
        : std::logic_error("promise error: " + description)
    {
    }
};

}

#endif // LEVELZ_PROMISE_ERROR_HPP
