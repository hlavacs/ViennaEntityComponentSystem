
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

using types = vtll::tl<int, bool, float, double>;
using sizes = vtll::tl< vtll::tl<int, vtll::vl<100>>, vtll::vl<100>>;
VECSSystem<types, 500, sizes> ECS;

void start_test() {

	auto handle = ECS.create<int,bool>(1, true);

}



int main() {

	start_test();

}