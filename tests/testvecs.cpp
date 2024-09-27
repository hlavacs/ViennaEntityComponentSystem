
#include <iostream>
#include <utility>
#include <ranges>
#include <utility>
#include "VECS.h"


int main() {
    vecs::Registry<vecs::PARALLEL> system;

    vecs::Handle h1 = system.create(5);
    assert( system.exists(h1) );
    auto t1 = system.types(h1);
    auto v1 = system.get<int>(h1);
    assert( system.has<int>(h1) );
    system.erase(h1);
    assert( !system.exists(h1) );
    system.validate();

    //vecs::Handle hx = system.create(5, 6); //compile error
    struct height_t { int i; }; 
    struct weight_t { int i; }; 
    vecs::Handle hx1 = system.create(5, height_t{6}, weight_t{6}); //works
    system.validate();

    auto h2 = system.create(5, 6.9f, 7.3);
    assert( system.exists(h2) );
    auto t2 = system.types(h2);
    auto [v2a, v2b] = system.get<float, double>(h2);
    system.validate();

    system.put(h2, 50, 69.0f, 73.0);
    assert( system.get<float>(h2) == 69.0f && system.get<double>(h2) == 73.0 );
    std::tuple<float,double> tup = system.get<float, double>(h2);
    std::get<float>(tup) = 101.0f;    
    std::get<double>(tup) = 102.0;    
    system.put(h2, tup);
    assert( system.get<float>(h2) == 101.0f && system.get<double>(h2) == 102.0 );
    system.validate();

    auto tup2 = system.get<int, float, double>(h2);
    int ii = std::get<int>(tup2);
    auto [ivalue, fvalue, dvalue] = system.get<int, float, double>(h2);
    system.validate();

    assert( system.has<int>(h2) );
    assert( system.has<float>(h2) );
    assert( system.has<double>(h2) );

    system.erase<int, float>(h2);
    assert( !system.has<int>(h2) );
    assert( !system.has<float>(h2) );
    assert( system.has<double>(h2) );
    system.erase<double>(h2); //remove also the last component
    assert( system.exists(h2)); //check that the entity still exists
    system.validate();

    system.erase(h2);
    assert( !system.exists(h2) );
    system.validate();

    auto hd1 = system.create(1, 10.0f, 10.0);
    auto hd2 = system.create(2, 20.0f );
    auto hd3 = system.create(3, 30.0, "AAA");
    auto hd4 = system.create(4, 40.0f, 40.0);
    auto hd5 = system.create(5);
    auto hd6 = system.create(6, 60.0f, 60.0);

    for( auto handle : system.view<vecs::Handle>() ) {
        std::cout << "Handle: "<< handle << std::endl;
    }

    for( auto [handle, i, f] : system.view<vecs::Handle, int, float>() ) {
        std::cout << "Handle: "<< handle << " int: " << i << " float: " << f << std::endl;
    }

        
    assert( system.size() > 0 );
    system.clear();
    assert( system.size() == 0 );

    return 0;   
}