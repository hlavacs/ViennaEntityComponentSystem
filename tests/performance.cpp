#include <iostream>
#include <utility>
#include <ranges>
#include <utility>
#include <random>
#include <iostream>
#include <string>
#include <random>
#include <chrono>

#include "VECS.h"

template<typename T>
void create_containers(size_t size, std::vector<vecs::Vector<T>> & containers) {
	size_t i = 0;
	for( size_t i = 0; i<=1.1*size; ++i ) {
		size_t j = 0;
		for( auto & container : containers ) {
			size_t r = (size * rand()) / RAND_MAX;
			container.push_back(T{.value = r});
		}
	}
}

template<typename T>
auto p1(size_t components, size_t size, std::vector<vecs::Vector<T>> & containers, bool seq = true) {
	volatile size_t sum = 0;
	volatile size_t index = 0;
	for( size_t i = 0; i < size; i++) {
		for( size_t j = 0; j < components; j++) {			
			volatile size_t value = containers[j][index].value;
			sum += value;
			index = seq ? i : value;
		}
	}
	return sum;
}

auto p2(size_t size) {

}

auto p3(size_t size) {

}


template<typename data>
void run() {
	size_t max_size = 100000;
	size_t repetitions = 200;

	std::vector<vecs::Vector<data>> containers {
		vecs::Vector<data>(10), vecs::Vector<data>(10), vecs::Vector<data>(10), vecs::Vector<data>(10), vecs::Vector<data>(10),
		vecs::Vector<data>(10), vecs::Vector<data>(10), vecs::Vector<data>(10), vecs::Vector<data>(10), vecs::Vector<data>(10)
	};

	create_containers(max_size, containers);

	std::cout << "subgroup,dataset,x,y" << std::endl;
	size_t factor = 5;
	volatile size_t sum = 0;
	for( size_t size = 100; size <= max_size;  ) {
		size_t components = 1;
		size_t cdelta = 5;
		
		for( ; components<=10; ) {

			for( size_t rep = 1; rep <= repetitions; ++rep) {

				auto t1 = std::chrono::high_resolution_clock::now();
				sum += p1(components, size, containers, true);
				auto t2 = std::chrono::high_resolution_clock::now();
				sum += p1(components, size, containers, false);
				auto t3 = std::chrono::high_resolution_clock::now();

				if( rep>=10 ) {
					std::cout << "Seq," << components << "C," << size << "," << std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count() << std::endl;
					std::cout << "Rnd," << components << "C," << size << "," << std::chrono::duration_cast<std::chrono::nanoseconds>(t3 - t2).count() << std::endl;
				}
			}

			components*=cdelta;
			cdelta = 2;
		}
		size *= factor;
		factor = factor == 5 ? 2 : 5;
	}
}

int main(int argc, char** argv) {
	struct data8 {
		size_t value;
	};
	struct data32 {
		size_t value;
		size_t pad[3];
	};
	struct data64 {
		size_t value;
		size_t pad[7];
	};

	run<data8>();
	//run<data32>();
	return 0;
}

