
#include <iostream>
#include <utility>
#include <chrono>
#include "glm.hpp"
#include "gtc/quaternion.hpp"

#include "performance.h"

#include "VECS.h"

using namespace vecs;

using namespace std::chrono;


void recursive() {

	bool test = true;

	auto f = [&]<typename B, typename E>(B && b, E && e, auto & self, int& i) {
		if (b == e) return false;
		auto [handle, pos] = *b;
		if (!handle.is_valid()) {
			return true;
		}

		auto ii = i;
		b++;
		while (self(b, e, self, ++i)) { --i; };

		pos.m_position = glm::vec3{ 4.0f + ii, 5.0f + ii, 6.0f + ii };

		return false;
	};

	VecsRange<VeComponentPosition> range;
	auto b = range.begin();
	auto e = range.end();
	int i = 1;
	while (f(b, e, f, i)) { b++; };

}


void linear(std::vector<VeComponentPosition>& vec) {

	int i = 0;
	/*if (vec.size() > 0) {
		for (auto&& [handle, pos] : VecsRegistry().begin<VeComponentPosition>()) {
			if (!handle.is_valid()) continue;
			++i;
			vec[i] = pos;
		}
	}*/

	i = 0;
	for (auto&& [handle, pos] : VecsRange<VeComponentPosition>{}) {
		if (!handle.is_valid()) continue;
		++i;
		pos.m_position = glm::vec3{ 7.0f + i, 8.0f + i, 9.0f + i };
	}
	
	int j = i;

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



int main() {

	VecsRegistry{};
	VecsRegistry<VeEntityTypeNode>{};
	VecsRegistry<VeEntityTypeDraw>{};
	VecsRegistry<VeEntityTypeAnimation>{};

	size_t num = 200000;
	std::vector<VeComponentPosition> vec{ 2*num };

	auto t0 = high_resolution_clock::now();
	init( num );

	auto t1 = high_resolution_clock::now();
	recursive();

	auto t2 = high_resolution_clock::now();
	linear(vec);

	auto t3 = high_resolution_clock::now();

	auto d1 = duration_cast<nanoseconds>(t1 - t0);
	auto d2 = duration_cast<nanoseconds>(t2 - t1);
	auto d3 = duration_cast<nanoseconds>(t3 - t2);

	double dt1 = d1.count() / 1.0;
	double dt2 = d2.count() / 1.0;
	double dt3 = d3.count() / 1.0;

	std::cout << "Init " << dt1 << " Recursive " << dt2 << " Linear " << dt3 << "\n";
	std::cout << dt1 / VecsRegistry().size() << " " << dt2 / VecsRegistry().size() << " " << dt3 / VecsRegistry().size() << "\n";

    return 0;
}

