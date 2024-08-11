
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
    auto v1 = system.get<int>(h1);
    bool b1a = system.has<int>(h1);
    system.erase<void>(h1);
    b1 = system.exists(h1);

    auto h2 = system.create(5, 6.9f, 7.3);
    bool b2 = system.exists(h2);
    auto t2 = system.types(h2);
    auto [v2a, v2b] = system.get<float, double>(h2);

    system.put(h2, 50, 69.0f, 73.0);
    auto tup = system.get<float, double>(h2);
    std::get<float>(tup) = 101.0f;    
    std::get<double>(tup) = 102.0;    
    system.put(h2, tup);
    tup = system.get<float, double>(h2);

    bool b2a = system.has<int>(h2);
    bool b2b = system.has<float>(h2);
    bool b2c = system.has<double>(h2);

    system.erase<int, float>(h2);
    b2a = system.has<int>(h2);
    b2b = system.has<float>(h2);
    b2c = system.has<double>(h2);

    system.erase<void>(h2);
    b2 = system.exists(h2);


    system.create(1, 10.0f, 10.0);
    system.create(2, 20.0f );
    system.create(3, 30.0);
    system.create(4, 40.0f, 40.0);
    system.create(5);
    system.create(6, 60.0f, 60.0);

    auto& comp = system.data<int>();
    for( auto& i : comp ) {
        std::cout << i.second << std::endl;
        if( system.has<float>(i.first) ) {
            std::cout << "Has float " << system.get<float>(i.first) << std::endl;
        }
    }


    return 0;   
}