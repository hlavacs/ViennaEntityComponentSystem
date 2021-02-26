
#include <iostream>
#include <iomanip>
#include <utility>
#include <string>
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

	VeComponentName name;
	VeComponentPosition pos{ glm::vec3{9.0f, 2.0f, 3.0f} };
	VeComponentOrientation orient{ glm::quat{glm::vec3{90.0f, 45.0f, 0.0f}} };
	VeComponentTransform trans{ glm::mat4{ 1.0f } };
	VeComponentMaterial mat{ 99 };
	VeComponentGeometry geo{ 11 };

	{
		TESTRESULT(++number, "insert", 
			auto h1 = VecsRegistry().insert(VeComponentName{ "Node" }, pos, orient, trans), 
				h1.has_value() && VecsRegistry().size() == 1, );

		TESTRESULT(++number, "insert<type>", 
			auto h2 = VecsRegistry<VeEntityTypeDraw>().insert(VeComponentName{ "Draw" }, mat, geo), 
				h2.has_value() && VecsRegistry().size() == 2, );
		
		TESTRESULT(++number, "component entity", auto ent1  = h1.entity<VeEntityTypeNode>().value(),	   
			(ent1.component<VeComponentPosition>().value().m_position == glm::vec3{ 9.0f, 2.0f, 3.0f }), );

		TESTRESULT(++number, "component entity", auto ent2 = h2.entity<VeEntityTypeDraw>().value(),
			(ent2.component<VeComponentMaterial>().value().i == 99), );

		TESTRESULT(++number, "component entity", auto ent3 = h1.entity<VeEntityTypeDraw>(), ent3.has_value(), );

		TESTRESULT(++number, "component entity", , (!ent1.component<VeComponentMaterial>().has_value()), );

		TESTRESULT(++number, "component handle", auto comp1 = h1.component<VeComponentPosition>().value(), 
			(comp1.m_position == glm::vec3{ 9.0f, 2.0f, 3.0f }), );

		TESTRESULT(++number, "component handle", auto comp2 = h1.component<VeComponentMaterial>(), (!comp2.has_value()), );

		TESTRESULT(++number, "local_update", ent1.local_update(VeComponentPosition{ glm::vec3{-99.0f, -22.0f, -33.0f} }),
			(ent1.component<VeComponentPosition>().value().m_position == glm::vec3{ -99.0f, -22.0f, -33.0f }), );

		TESTRESULT(++number, "update entity", ent1.update(), (h1.component<VeComponentPosition>().value().m_position == glm::vec3{ -99.0f, -22.0f, -33.0f }), );

		TESTRESULT(++number, "local_update",  ent1.local_update<VeComponentPosition>(VeComponentPosition{ glm::vec3{-9.0f, -2.0f, -3.0f} }),
			(ent1.component<VeComponentPosition>().value().m_position == glm::vec3{ -9.0f, -2.0f, -3.0f }), );

		TESTRESULT(++number, "update entity", ent1.update(), (h1.component<VeComponentPosition>().value().m_position == glm::vec3{ -9.0f, -2.0f, -3.0f }), );

		TESTRESULT(++number, "update handle", h1.update(VeComponentPosition{ glm::vec3{99.0f, 22.0f, 33.0f} }),
			(h1.component<VeComponentPosition>().value().m_position == glm::vec3{ 99.0f, 22.0f, 33.0f }), );

		TESTRESULT(++number, "update handle", h1.update<VeComponentPosition>(VeComponentPosition{ glm::vec3{-99.0f, -22.0f, -33.0f} }),
			(h1.component<VeComponentPosition>().value().m_position == glm::vec3{ -99.0f, -22.0f, -33.0f }), );

		TESTRESULT(++number, "erase handle", h1.erase(), (!h1.has_value() && VecsRegistry().size() == 1), );
		TESTRESULT(++number, "erase handle", h2.erase(), (!h2.has_value() && VecsRegistry().size() == 0), );

		int i = 0;
		bool test = true;
		for_each<VeComponentName>([&](auto& iter) {
			++i;
			auto [handle, name] = *iter;
			if (name.m_name != "Node" && name.m_name != "Draw") { test = false; }
			//std::cout << "Entity " << name << "\n";
			});
		TESTRESULT(++number, "system create", , (test && i == 0), );
	}

	{
		for (int i = 0; i < 1000; i++) {
			auto h1 = VecsRegistry().insert(VeComponentName{ "Node" }, VeComponentPosition{}, VeComponentOrientation{}, VeComponentTransform{});
			auto h2 = VecsRegistry().insert(VeComponentName{ "Draw" }, VeComponentMaterial{ 1 }, VeComponentGeometry{ 1 });
		}
		TESTRESULT(++number, "system create", , (VecsRegistry().size() == 2000), );

		int i = 0;
		bool test = true;
		for_each<VeComponentName>([&](auto& iter) {
			++i;
			auto [handle, name] = *iter;
			if (name.m_name != "Node" && name.m_name != "Draw") {	test = false; }
			//std::cout << "Entity " << name << "\n";
		});
		TESTRESULT(++number, "system run", , (test && i == 2000), );

		TESTRESULT(++number, "clear", VecsRegistry().clear(), (VecsRegistry().size() == 0), );

	}


    return 0;
}

