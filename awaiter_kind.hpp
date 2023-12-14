//
// Created by irantha on 12/7/23.
//

#ifndef LEVELZ_AWAITER_KIND_HPP
#define LEVELZ_AWAITER_KIND_HPP

namespace Levelz::Async {

enum class AwaiterKind {
    Initial,
    Final,
    Yield,
    Task,
    Mutex,
    Event,
    Value,
    Barrier,
    ThreadPool
};

}

#endif // LEVELZ_AWAITER_KIND_HPP
