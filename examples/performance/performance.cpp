
#include <iostream>
#include <utility>
#include <chrono>
#include "glm.hpp"
#include "gtc/quaternion.hpp"
#include "VECS.h"
#include "performance.h"

using namespace vecs;


void recursive() {


	bool test = true;

	auto f = [&]<typename B, typename E>(B && b, E && e, auto & self, int& i) {

		if (b == e) return false;
		auto [handle, pos] = *b;
		if (!handle.is_valid()) {
			return true;
		}

		auto ii = i;
		while (self(b++, e, self, ++i)) { --i; };

		return false;
	};

	auto b = VecsRegistry().begin<VeComponentPosition>();
	auto e = VecsRegistry().end<VeComponentPosition>();
	int i = 1;
	while (f(b, e, f, i)) { b++; };


}






void init() {
	VeComponentName name;
	VeComponentPosition pos{ glm::vec3{9.0f, 2.0f, 3.0f} };
	VeComponentOrientation orient{ glm::quat{glm::vec3{90.0f, 45.0f, 0.0f}} };
	VeComponentTransform trans{ glm::mat4{ 1.0f } };
	VeComponentMaterial mat{ 99 };
	VeComponentGeometry geo{ 11 };

	auto h = VecsRegistry<VeEntityTypeNode>().insert(name, pos, orient, trans);

}






int main() {

	VecsRegistry{};
	VecsRegistry<VeEntityTypeNode>{};
	VecsRegistry<VeEntityTypeDraw>{};
	VecsRegistry<VeEntityTypeAnimation>{};

	init();
	recursive();

    return 0;
}

