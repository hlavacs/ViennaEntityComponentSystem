
#include <iostream>
#include <iomanip>
#include <utility>
#include "glm.hpp"
#include "gtc/quaternion.hpp"
#include "VECS.h"
#include "test.h"

using namespace vecs;

#define TESTRESULT(N, S, EXPR, B, C) \
		EXPR; \
		std::cout << "Test " << std::right << std::setw(3) << N << "  " << std::left << std::setw(30) << S << " " << ( B ? "PASSED":"FAILED" ) << std::endl;\
		C;


int main() {
	int number = 0;
	std::atomic<int> counter = 0;

	VeComponentPosition pos{ glm::vec3{9.0f, 2.0f, 3.0f} };
	VeComponentOrientation orient{ glm::quat{glm::vec3{90.0f, 45.0f, 0.0f}} };
	VeComponentTransform trans{ glm::mat4{ 1.0f } };

	//TESTRESULT(++number, "Single function", co_await[&]() { func(&counter); }, counter.load() == 1, counter = 0);
	//TESTRESULT(++number, "10 functions", co_await[&]() { func(&counter, 10); }, counter.load() == 10, counter = 0);

	TESTRESULT(++number, "insert", auto h1 = VecsRegistry().insert(pos, orient, trans), VecsRegistry().size() == 1, );




    return 0;
}

