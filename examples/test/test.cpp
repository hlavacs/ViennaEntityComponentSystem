
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

	auto handle1 = ECS.create<size_t,bool>(1ul, true);
	auto handle2 = ECS.create<bool, float, double>(true, 1.0f, 2.0);
	auto handle3 = ECS.create<bool, double>(false, 4.0);

	auto a = ECS.component<size_t>(handle1);
	auto b = ECS.component<bool>(handle1);
	auto c = ECS.component<float>(handle1);

	auto d = ECS.container<size_t>(handle1);

}



int main() {

	start_test();

}