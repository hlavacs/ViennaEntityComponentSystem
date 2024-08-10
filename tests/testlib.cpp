
#include <iostream>
#include <utility>
#include <ranges>
#include <utility>
#include "VECS.h"

using namespace vecs;


int main() {
    VecsSystem system;

    VecsHandle h1 = system.create(5);
    bool b1 = system.exists(h1);
    auto t1 = system.types(h1);
    auto& v1 = system.get<int>(h1);
    bool b1a = system.has<int>(h1);
    system.erase<void>(h1);
    b1 = system.exists(h1);

    auto h2 = system.create(5, 6.9f, 7.3);
    bool b2 = system.exists(h2);
    auto t2 = system.types(h2);
    auto v2 = system.get<float>(h2);

    bool b2a = system.has<int>(h2);
    bool b2b = system.has<float>(h2);
    bool b2c = system.has<double>(h2);

    system.erase<int, float>(h2);
    b2a = system.has<int>(h2);
    b2b = system.has<float>(h2);
    b2c = system.has<double>(h2);

    system.erase<void>(h2);
    b2 = system.exists(h2);




    return 0;   
}