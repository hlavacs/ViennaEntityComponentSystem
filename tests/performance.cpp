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
void refill_containers(size_t size, std::vector<vecs::Vector<T>> & containers) {
    std::random_device rd;  // Hardware-based random seed
	std::mt19937 gen(rd()); // Mersenne Twister generator   
    std::uniform_int_distribution<size_t> int_dist(0ul, (size_t)100*size);

	size_t i = 0;
	for( size_t i = 0; i<size; ++i ) {
		size_t j = 0;
		for( auto & container : containers ) {
			container.push_back(T{.value = (size_t)int_dist(gen)%size});
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

template<typename data>
void run() {
	size_t max_size = 102400;
	size_t repetitions = 200;

	constexpr size_t BITS = 10ul;
	std::vector<vecs::Vector<data>> containers {
		vecs::Vector<data>(BITS), vecs::Vector<data>(BITS), vecs::Vector<data>(BITS), vecs::Vector<data>(BITS), vecs::Vector<data>(BITS),
		vecs::Vector<data>(BITS), vecs::Vector<data>(BITS), vecs::Vector<data>(BITS), vecs::Vector<data>(BITS), vecs::Vector<data>(BITS),
		vecs::Vector<data>(BITS), vecs::Vector<data>(BITS), vecs::Vector<data>(BITS), vecs::Vector<data>(BITS), vecs::Vector<data>(BITS)
	};

	std::cout << "subgroup,dataset,x,y" << std::endl;
	volatile size_t sum = 0;

	refill_containers(max_size, containers);

	for( size_t size = 1024; size <= max_size;  ) {
	
		for( size_t rep = 1; rep <= repetitions; ++rep) {
			
			refill_containers(size, containers);
		
			size_t cdelta = 3;

			for( size_t components = 1; components<=10; ) {

				auto t1 = std::chrono::high_resolution_clock::now();
				sum += p1(components, size, containers, true);
				auto t2 = std::chrono::high_resolution_clock::now();
				sum += p1(components, size, containers, false);
				auto t3 = std::chrono::high_resolution_clock::now();

				if( rep>=20 ) {
					std::cout << "Seq," << std::setw(2) << components << "C," << size << "," << std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count() << std::endl;
					std::cout << "Rnd," << std::setw(2) << components << "C," << size << "," << std::chrono::duration_cast<std::chrono::nanoseconds>(t3 - t2).count() << std::endl;
				}

				components+=cdelta;
				cdelta = 2;
			}
		}
		size += (size>=10240) ? 10240 : 1024;
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

