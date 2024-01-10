
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
	assert(ECS.valid(handle1));

	auto handle2 = ECS.template insert<int,char,std::string,float,double>(1, 'b', "CCCCDDD", 1.0f, 2.0);
	assert(ECS.valid(handle2));

	auto handle3 = ECS.template insert<std::string,float,double>("EEFFF", 3.0f, 4.0);
	assert(ECS.valid(handle3));

	vecs::VecsIndex index{};
	vecs::VecsHandle handle{};

	vecs::VecsIndex i1{handle};
	//vecs::VecsHandle h1{index};  //compiler error


}



int main() {

	vecs::VecsSystem<vtll::tl<int, char, std::string, float, double>> ECS{};

	start_test( ECS );

	return 0;
}

