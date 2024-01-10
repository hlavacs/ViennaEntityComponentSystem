
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
	vecs::VecsIndex index{};
	vecs::VecsHandle handle{};
	vecs::VecsIndex i1{handle};
	//vecs::VecsHandle h1{index};  //compiler error

	auto handle1 = ECS.template insert<int,char,std::string>(0, 'a', "ABAB");
	assert(ECS.valid(handle1));

	auto handle2 = ECS.template insert<int,char,std::string,float,double>(1, 'b', "CCCCDDD", 1.0f, 2.0);
	assert(ECS.valid(handle2));

	auto handle3 = ECS.template insert<std::string,float,double>("EEFFF", 3.0f, 4.0);
	assert(ECS.valid(handle3));

	auto value1 = ECS.template get<int,char,std::string>(handle1);
	if( value1.has_value() ) {
		auto tuple = value1.value();
		std::cout << "value1: " << std::get<0>(tuple) << " " << std::get<1>(tuple) << " " << std::get<2>(tuple) << std::endl;
	}

	auto value2 = ECS.template get<int,char,std::string,float,double>(handle2);
	if( value2.has_value() ) {
		auto tuple = value2.value();
		std::cout << "value2: " << std::get<0>(tuple) << " " << std::get<1>(tuple) << " " << std::get<2>(tuple) << " " << std::get<3>(tuple) << " " << std::get<4>(tuple) << std::endl;
	}

	auto value3 = ECS.template get<std::string,float,double>(handle3);
	if( value3.has_value() ) {
		auto tuple = value3.value();
		std::cout << "value3: " << std::get<0>(tuple) << " " << std::get<1>(tuple) << " " << std::get<2>(tuple) << std::endl;
	}

	assert( ECS.erase(handle1) );
	auto value4 = ECS.template get<int,char,std::string>(handle1);
	assert(value4.has_value() == false);
	
}



int main() {

	vecs::VecsSystem<vtll::tl<int, char, std::string, float, double>> ECS{};

	start_test( ECS );

	return 0;
}

