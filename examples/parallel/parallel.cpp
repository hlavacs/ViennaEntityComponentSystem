
#include <iostream>
#include <iomanip>
#include <utility>
#include <string>
#include <atomic>
#include "glm.hpp"
#include "gtc/quaternion.hpp"

#include "VGJS.h"
#include "VGJSCoro.h"

#include "parallel.h"

#include "VECS.h"


using namespace vecs;


std::atomic<size_t> cnt = 0;

template<typename it> 
void linear( it&& b, it&& e) {

	int i = 0;

	while ( b != e ) {
		auto&& [handle, pos] = *b;
		if (!handle.is_valid()) continue;
		++i;
		pos.m_position = glm::vec3{ 7.0f + i, 8.0f + i, 9.0f + i };
		++b;
		cnt++;
	}

	//std::cout << i << "\n";

}


void init(size_t num) {

	for (int i = 0; i < num; ++i) {
		VeComponentName name;
		VeComponentPosition pos{ glm::vec3{1.0f + i, 2.0f + i, 3.0f + i} };
		VeComponentOrientation orient{ glm::quat{glm::vec3{90.0f, 45.0f, 0.0f + i}} };
		VeComponentTransform trans{ glm::mat4{ 1.0f } };
		VeComponentMaterial mat{ 99 };
		VeComponentGeometry geo{ 11 };

		auto h1 = VecsRegistry<VeEntityTypeNode>().insert(name, pos, orient, trans);
		auto h2 = VecsRegistry<VeEntityTypeDraw>().insert(name, pos, orient, trans, mat, geo);
	}
}


vgjs::Coro<> start() {
    std::cout << "Start \n";

	VecsRegistry{ 1 << 23 };
	VecsRegistry<VeEntityTypeNode>{ 1 << 20 };
	VecsRegistry<VeEntityTypeDraw>{ 1 << 20 };
	VecsRegistry<VeEntityTypeAnimation>{ 1 << 20 };

	size_t num = 300000;

	init(num);

	int thr = 1;
	std::pmr::vector<vgjs::Function> vec;
	auto bi = VecsRegistry().begin<VeComponentPosition>();
	auto ei = VecsRegistry().end<VeComponentPosition>();

	auto b = VecsRegistry().begin<VeComponentPosition>();
	auto e = VecsRegistry().begin<VeComponentPosition>();

	for (int i = 0; i < thr; ++i) {
		b = bi + i * bi.size() / thr;
		size_t sizeit = bi.size();
		if (i < thr - 1) {
			e = bi + (i + 1) * bi.size() / thr;
			vec.push_back(vgjs::Function([&]() { linear(b, e); }));
		}
		else {
			vec.push_back(vgjs::Function([&]() { linear(b, ei); }));
		}
	}


	auto t0 = high_resolution_clock::now();

	//linear( VecsRegistry().begin<VeComponentPosition>(), VecsRegistry().end<VeComponentPosition>());

	auto t1 = high_resolution_clock::now();

	co_await parallel(vec);

	auto t2 = high_resolution_clock::now();

	auto d1 = duration_cast<nanoseconds>(t1 - t0);
	auto d2 = duration_cast<nanoseconds>(t2 - t1);

	double dt1 = d1.count() / 1.0;
	double dt2 = d2.count() / 1.0;

	size_t size = VecsRegistry().size();

	std::cout << "Num " << cnt << "\n";
	std::cout << "Linear " << dt1 << " Parallel " << dt2 << "\n";
	std::cout << dt1 / cnt << " " << dt2 / cnt << "\n";

    vgjs::terminate();
    co_return;
}



int main() {

	vgjs::JobSystem();

    vgjs::schedule( start() );

    vgjs::wait_for_termination();

    return 0;
}

