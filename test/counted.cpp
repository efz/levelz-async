//
// Created by irantha on 5/12/23.
//

#include "counted.hpp"

namespace Levelz::Async::Test {

std::atomic<int> Counted::s_defaultConstructionCount;
std::atomic<int> Counted::s_copyConstructionCount;
std::atomic<int> Counted::s_copyAssignmentCount;
std::atomic<int> Counted::s_moveConstructionCount;
std::atomic<int> Counted::s_moveAssignmentCount;
std::atomic<int> Counted::s_destructionCount;

}