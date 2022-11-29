
#include <iostream>
#include <iomanip>
#include <utility>
#include <string>
#include <vector>
#include <algorithm>
#include <numeric>
#include <mutex>

#include "VECS.h"
#include "test.h"

#include <future>

using namespace vecs;

using types = vtll::tl<size_t, bool, float, double>;
using sizes = vtll::tl< vtll::tl<size_t, vtll::vl<100> > >;
VECSSystem<types, 500, sizes> ECS;

void start_test() {

	auto handle = ECS.create<size_t,bool>(1ul, true);

}



int main() {

	start_test();

}