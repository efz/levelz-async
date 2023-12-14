//
// Created by irantha on 12/7/23.
//

#ifndef LEVELZ_TASK_KIND_HPP
#define LEVELZ_TASK_KIND_HPP

namespace Levelz::Async {

enum class TaskKind {
    Simple,
    Task,
    Async,
    Sync
};

}

#endif // LEVELZ_TASK_KIND_HPP
