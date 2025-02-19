#include <iostream>
#include <utility>
#include <ranges>
#include <utility>
#include <random>
#include <print>

//#include "VECS.h"

#include <VECSHandle.h>
#include <VECSVector.h>

void test_handle() {
	std::print("\x1b[37m testing handle...");
	vecs::Handle h0;
	vecs::Handle h1{1,2};
	vecs::Handle h2{1,2};
	vecs::Handle h3{1,3};

	assert( h0.IsValid() == false );
	assert( h1.IsValid() == true );
	assert( h1.GetIndex() == 1 );
	assert( h1.GetVersion() == 2 );
	assert( h1.GetStorageIndex() == 0 );
	assert( h1 == h2 );
	assert( h1 != h3 );
	assert( h2 != h3 );
	std::print("\x1b[32m passed\n");
}

void test_vector() {
	std::print("\x1b[37m testing vector...");
	{
		vecs::Vector<int> vec;
		vec.push_back( 0 );
		assert( vec[0] == 0 );
		assert( vec.size() == 1 );
		for( int i=1; i<10000; ++i ) { 
			//std::print("insert {}\n", i); 
			vec.push_back( i ); 
		}
		assert( vec.size() == 10000 );
		for( int i=0; i<10000; ++i ) { 
			//std::print("assert {}\n", i); 
			assert( vec[i] == i ); 
		}
		int j=0;
		for( auto& i : vec ) { assert( i == j++ ); }
	}
	std::print("\x1b[32m passed\n");
}

void test_slotmap() {

}

void test_hashmap() {

}

void test_archetype() {

}

void test_mutex() {

}

void test_registry() {

}


int main() {
	test_handle();
	test_vector();
	test_slotmap();
	test_hashmap();
	test_archetype();
	test_mutex();
	test_registry();
}

