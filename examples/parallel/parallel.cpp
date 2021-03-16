
#include <iostream>
#include <iomanip>
#include <utility>
#include <string>
#include "glm.hpp"
#include "gtc/quaternion.hpp"

#include "VGJS.h"
#include "VGJSCoro.h"

#include "parallel.h"

#include "VECS.h"


using namespace vecs;




vgjs::Coro<> start() {
    std::cout << "Start \n";



    vgjs::terminate();
    co_return;
}



int main() {

    vgjs::schedule( start() );

    vgjs::wait_for_termination();

    return 0;
}

