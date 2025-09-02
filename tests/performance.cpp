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
			container.push_back(T{.value = i + (j++)});
		}
	}
}

template<typename T>
void p1(size_t components, size_t size, std::vector<vecs::Vector<T>> & containers, bool seq = true) {
	for( size_t j = 0; j < components; j++) {
		for( size_t i = 0; i < size; i++) {
			volatile size_t r = size * rand() / RAND_MAX;
			volatile size_t k = seq ? i : r;
						
			containers[j][k].value++;
		}
	}
}

void p2(size_t size) {

}

void p3(size_t size) {

}


template<typename data>
void run() {
	size_t max_size = 100000;

	std::vector<vecs::Vector<data>> containers {
		vecs::Vector<data>(), vecs::Vector<data>(), vecs::Vector<data>(), vecs::Vector<data>(), vecs::Vector<data>(),
		vecs::Vector<data>(), vecs::Vector<data>(), vecs::Vector<data>(), vecs::Vector<data>(), vecs::Vector<data>()
	};

	create_containers(max_size, containers);

	size_t factor = 5;

	std::cout << "SEQ COMPONENTS SIZE NS\n";
	for( size_t size = 100; size <= max_size;  ) {
		for( size_t components = 1; components<=10; components++) {

			auto t1 = std::chrono::high_resolution_clock::now();
			p1(components, size, containers, true);
			auto t2 = std::chrono::high_resolution_clock::now();
			p1(components, size, containers, false);
			auto t3 = std::chrono::high_resolution_clock::now();
			std::cout << "Sequential " << components << " " << size << " " << std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count() << "\n";
			std::cout << "Random " << components << " " << size << " " << std::chrono::duration_cast<std::chrono::nanoseconds>(t3 - t2).count() << "\n";

		}
		size *= factor;
		factor = factor == 5 ? 2 : 5;
	}
}

int main(int argc, char** argv) {
	struct data {
		size_t value;
	};

	run<data>();
	return 0;
}

