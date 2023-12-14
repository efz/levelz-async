//
// Created by irantha on 12/11/23.
//

#ifndef LEVELZ_THREAD_POOL_KIND_HPP
#define LEVELZ_THREAD_POOL_KIND_HPP

namespace Levelz::Async {

enum class ThreadPoolKind {
    Current,
    Default,
    Background
};

}

#endif // LEVELZ_THREAD_POOL_KIND_HPP
