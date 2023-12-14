//
// Created by irantha on 6/19/23.
//

#ifndef LEVELZ_CANCELLATION_ERROR_HPP
#define LEVELZ_CANCELLATION_ERROR_HPP

#include <stdexcept>
#include <string>

namespace Levelz::Async {

struct CancellationError : std::runtime_error {
    CancellationError()
        : std::runtime_error("cancellation error")
    {
    }

    explicit CancellationError(const std::string& description)
        : std::runtime_error("cancellation error: " + description)
    {
    }

    static CancellationError shutdownCancellationError() {
        return CancellationError("shutting down");
    }
};

}

#endif // LEVELZ_CANCELLATION_ERROR_HPP
