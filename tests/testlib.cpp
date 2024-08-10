
#include <iostream>
#include <utility>
#include <ranges>
#include <utility>
#include "VECS.h"

using namespace vecs;


int main() {
    VecsSystem system;

    VecsHandle handle = system.create(5);
    auto h1 = system.create(5, 6.9f, 7.3);
    return 0;   
}