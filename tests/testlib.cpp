
#include <iostream>
#include <iomanip>
#include <utility>
#include <string>
#include <vector>
#include <algorithm>
#include <numeric>
#include <mutex>

#include "VECS.h"

using outgroup = vtll::tl<size_t, bool, float, double>;


void start_test( auto & ECS) {
	auto handle1 = ECS.template insert<int,char,std::string>(0, 'a', "ABAB");
}



int main() {

	vecs::VecsSystem<vtll::tl<int, char, std::string, float, double>> ECS{};

	start_test( ECS );

	return 0;
}

