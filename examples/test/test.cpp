
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
VECSSystem<types> ECS;


void start_test() {

	auto handle1 = ECS.create<size_t,bool>(1ul, true);
	auto handle2 = ECS.create<bool, float, double>(true, 1.0f, 2.0);
	auto handle3 = ECS.create<bool, double>(false, 4.0);

	auto a = ECS.get<size_t,bool>(handle1);
	auto b = ECS.get<bool>(handle1);
	auto c = ECS.get<float>(handle1);

	ECS.erase(handle1);
	bool v = ECS.valid(handle1);
	auto d = ECS.get<size_t>(handle1);

	for (auto it : ECS.range<size_t, bool>()) {
		auto val = it;
	}
}



int main() {

	start_test();

}