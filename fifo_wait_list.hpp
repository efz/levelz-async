//
// Created by irantha on 5/27/23.
//

#ifndef LEVELZ_FIFO_WAIT_LIST_HPP
#define LEVELZ_FIFO_WAIT_LIST_HPP

#include "concurrent_fifo_list.hpp"

namespace Levelz::Async {

struct Coroutine;

using FifoWaitList = ConcurrentFifoList<Coroutine>;

}

#endif // LEVELZ_FIFO_WAIT_LIST_HPP
