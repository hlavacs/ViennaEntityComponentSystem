
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

using ingroup = vtll::tl<>;
using outgroup = vtll::tl<size_t, bool, float, double>;
VECSSystem<ingroup, outgroup> ECS;


void start_test() {

	auto handle1 = ECS.create<size_t,bool>(1ul, true);
	auto handle2 = ECS.create<bool, float, double>(true, 10.0f, 100.0);
	auto handle3 = ECS.create<bool, double>(false, 101.0);

	auto handle4 = ECS.create<size_t, bool>(2ul, false);
	auto handle5 = ECS.create<bool, float, double>(true, 11.0f, 102.0);
	auto handle6 = ECS.create<bool, double>(false, 103.0);

	/*for (auto it : ECS.range<size_t, bool>()) {
		auto val = it;
		std::cout << std::get<0>(val.first) << "\n";
	}

	for (auto it : ECS.range<size_t>()) {
		auto val = it;
		std::cout << it.first << "\n";
	}

	for (auto it : ECS.range<bool>()) {
		auto val = it;
		std::cout << it.first << "\n";
	}

	for (auto it : ECS.range<float>()) {
		auto val = it;
		std::cout << it.first << "\n";
	}

	for (auto it : ECS.range<double>()) {
		auto val = it;
		std::cout << it.first << "\n";
	}

	auto a = ECS.get<size_t,bool>(handle1);
	auto b = ECS.get<bool>(handle1);
	auto c = ECS.get<float>(handle1);

	ECS.erase(handle1);
	bool v = ECS.valid(handle1);
	auto d = ECS.get<size_t>(handle1);

	for (auto it : ECS.range<size_t, bool>()) {
		auto val = it;
		std::cout << std::get<1>(val.first) << "\n";
	}
	*/
}



int main() {

	start_test();

}