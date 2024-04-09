
#include <iostream>
#include <iomanip>
#include <utility>
#include <string>
#include <vector>
#include <algorithm>
#include <numeric>
#include <mutex>

#define VecsUserEntityComponentList vtll::tl<int, std::string, float, double>

#include "VECS.h"


void start_test( auto & ECS) {
	vecs::VecsIndex index{};
	vecs::VecsHandle handle{};
	vecs::VecsIndex i1{handle};
	//vecs::VecsHandle h1{index};  //compiler error

	auto handle1 = ECS.template insert<int,float,std::string>(0, 4.5f, std::string("ABAB"));
	assert(ECS.valid(handle1));
	auto handle1a = ECS.template insert<int,float,std::string>(2, 5.5f, std::string("ffff"));
	assert(ECS.valid(handle1a));
	auto handle1b = ECS.template insert<int,float,std::string>(3, 6.5f, std::string("hhhhh"));
	assert(ECS.valid(handle1b));

	auto handle2 = ECS.template insert<int,std::string,float,double>(1, "CCCCDDD", 1.0f, 2.0);
	assert(ECS.valid(handle2));

	auto handle3 = ECS.template insert<std::string,float,double>("EEFFF", 3.0f, 4.0);
	assert(ECS.valid(handle3));

	{
		auto value1 = ECS.template get<int,std::string>(handle1);
		if( value1.has_value() )  {
			auto tuple = value1.value();
			std::cout << "value1:: " << std::get<0>(tuple) << " " << std::get<1>(tuple) << " " << std::endl;
		}	

		auto value2 = ECS.template get<int,std::string,float,double>(handle2);
		if( value2.has_value() ) {
			auto tuple = value2.value();
			std::cout << "value2: " << std::get<0>(tuple) << " " << std::get<1>(tuple) << " " << std::get<2>(tuple) << " " << std::get<3>(tuple) << " " << std::endl;
		}

		auto value3 = ECS.template get<std::string,float,double>(handle3);
		if( value3.has_value() ) {
			auto tuple = value3.value();
			std::cout << "value3: " << std::get<0>(tuple) << " " << std::get<1>(tuple) << " " << std::get<2>(tuple) << std::endl;
		}
	}
	
	assert( ECS.erase(handle1) );
	auto value4 = ECS.template get<int,std::string>(handle1);
	assert(value4.has_value() == false);

	{
		ECS.template transform<int,std::string>(handle3, 2, std::string("NNNNNNNN") );
		auto value3 = ECS.template get<int,std::string>(handle3);
		if( value3.has_value() ) {
			auto tuple = value3.value();	
			std::cout << "value3: " << std::get<0>(tuple) << " " << std::get<1>(tuple) << " " << std::endl;	
		}
	}
	
}



int main() {

	vecs::VecsSystem<VecsUserEntityComponentList> ECS{};

	start_test( ECS );

	return 0;
}

