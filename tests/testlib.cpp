
#include <iostream>
#include <utility>
#include <ranges>
#include <utility>
#include "VECS.h"


int main() {
    vecs::Registry system;

    vecs::Handle h1 = system.create(5);
    assert( system.exists(h1) );
    auto t1 = system.types(h1);
    auto v1 = system.get<int>(h1);
    assert( system.has<int>(h1) );
    system.erase(h1);
    assert( !system.exists(h1) );
    system.validate();

    auto h2 = system.create(5, 6.9f, 7.3);
    assert( system.exists(h2) );
    auto t2 = system.types(h2);
    auto [v2a, v2b] = system.get<float, double>(h2);
    system.validate();

    system.put(h2, 50, 69.0f, 73.0);
    auto tup = system.get<float, double>(h2);
    std::get<float>(tup) = 101.0f;    
    std::get<double>(tup) = 102.0;    
    system.put(h2, tup);
    auto tup2 = system.get<int, float, double>(h2);
    int ii = std::get<int>(tup2);
    system.validate();

    assert( system.has<int>(h2) );
    assert( system.has<float>(h2) );
    assert( system.has<double>(h2) );

    system.erase<int, float>(h2);
    assert( !system.has<int>(h2) );
    assert( !system.has<float>(h2) );
    assert( system.has<double>(h2) );

    system.erase(h2);
    assert( !system.exists(h2) );
    system.validate();

    system.create(1, 10.0f, 10.0);
    system.create(2, 20.0f );
    system.create(3, 30.0, "AAA");
    system.create(4, 40.0f, 40.0);
    system.create(5);
    system.create(6, 60.0f, 60.0);

   
    auto view = system.view<vecs::Handle, int, float>();

    for( auto it : view ) {
        auto [handle, i, f] = it;
        std::cout << "Handle: "<< handle << " int: " << i << " float: " << f << std::endl;
    }


    return 0;   
}