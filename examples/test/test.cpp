
#include <iostream>
#include <iomanip>
#include <utility>
#include <string>
#include "glm.hpp"
#include "gtc/quaternion.hpp"

#include "test.h"

#include "VECS.h"

using namespace vecs;

#define TESTRESULT(N, S, EXPR, B, C) \
		EXPR; \
		std::cout << "Test " << std::right << std::setw(3) << N << "  " << std::left << std::setw(30) << S << " " << ( B ? "PASSED":"FAILED" ) << std::endl;\
		C;


int main() {
	int number = 0;
	std::atomic<int> counter = 0;

	VecsRegistry{};
	VecsRegistry<VeEntityTypeNode>{};
	VecsRegistry<VeEntityTypeDraw>{};

	VeComponentName name;
	VeComponentPosition pos{ glm::vec3{9.0f, 2.0f, 3.0f} };
	VeComponentOrientation orient{ glm::quat{glm::vec3{90.0f, 45.0f, 0.0f}} };
	VeComponentTransform trans{ glm::mat4{ 1.0f } };
	VeComponentMaterial mat{ 99 };
	VeComponentGeometry geo{ 11 };

	{
		TESTRESULT(++number, "insert", 
			auto h1 = VecsRegistry{}.insert<VeEntityTypeNode>(VeComponentName{ "Node" }, pos, orient, trans),
				h1.has_value() && VecsRegistry().size() == 1, );

		TESTRESULT(++number, "insert<type>", 
			auto h2 = VecsRegistry<VeEntityTypeDraw>{}.insert(VeComponentName{ "Draw" }, pos, orient, trans, mat, geo),
				h2.has_value() && VecsRegistry().size() == 2, );

		TESTRESULT(++number, "insert per proxy",
			auto pr1 = VecsEntityProxy<VeEntityTypeDraw>(VeComponentName{ "Draw" }, pos, orient, trans, mat, geo),
			pr1.has_value() && VecsRegistry().size() == 3, );

		TESTRESULT(++number, "component entity", auto ent1  = h1.proxy<VeEntityTypeNode>(),
			(ent1.component<VeComponentPosition>().m_position == glm::vec3{ 9.0f, 2.0f, 3.0f }), );

		TESTRESULT(++number, "component entity", auto ent2 = h2.proxy<VeEntityTypeDraw>(),
			(ent2.component<VeComponentMaterial>().i == 99), );

		TESTRESULT(++number, "component entity", auto ent3 = h1.proxy<VeEntityTypeDraw>(), !ent3.has_value(), );

		TESTRESULT(++number, "component entity", auto ent4 = h1.proxy<VeEntityTypeDraw>(true), !ent4.has_value(), );

		TESTRESULT(++number, "component handle", auto comp1 = h1.component<VeComponentPosition>().value(), 
			(comp1.m_position == glm::vec3{ 9.0f, 2.0f, 3.0f }), );

		TESTRESULT(++number, "component handle", auto comp2 = h1.component<VeComponentMaterial>(), (!comp2.has_value()), );

		TESTRESULT(++number, "component handle", auto comp3 = h2.component<VeComponentMaterial>().value(), (comp3.i == 99), );

		TESTRESULT(++number, "local_update", ent1.local_update(VeComponentPosition{ glm::vec3{-99.0f, -22.0f, -33.0f} }),
			(ent1.component<VeComponentPosition>().m_position == glm::vec3{ -99.0f, -22.0f, -33.0f }), );

		TESTRESULT(++number, "update entity", ent1.update(), (h1.component<VeComponentPosition>().value().m_position == glm::vec3{ -99.0f, -22.0f, -33.0f }), );

		TESTRESULT(++number, "local_update",  ent1.local_update<VeComponentPosition>(VeComponentPosition{ glm::vec3{-9.0f, -2.0f, -3.0f} }),
			(ent1.component<VeComponentPosition>().m_position == glm::vec3{ -9.0f, -2.0f, -3.0f }), );

		TESTRESULT(++number, "update entity", ent1.update(), (h1.component<VeComponentPosition>().value().m_position == glm::vec3{ -9.0f, -2.0f, -3.0f }), );

		TESTRESULT(++number, "update handle", h1.update(VeComponentPosition{ glm::vec3{99.0f, 22.0f, 33.0f} }),
			(h1.component<VeComponentPosition>().value().m_position == glm::vec3{ 99.0f, 22.0f, 33.0f }), );

		TESTRESULT(++number, "update handle", h1.update<VeComponentPosition>(VeComponentPosition{ glm::vec3{-99.0f, -22.0f, -33.0f} }),
			(h1.component<VeComponentPosition>().value().m_position == glm::vec3{ -99.0f, -22.0f, -33.0f }), );

		TESTRESULT(++number, "erase handle per entity", pr1.erase(), (!pr1.has_value() && VecsRegistry().size() == 2), );
		TESTRESULT(++number, "size", , (VecsRegistry().size<VeEntityTypeDraw>() == 1), );

		TESTRESULT(++number, "erase handle", h1.erase(), (!h1.has_value() && VecsRegistry().size() == 1), );
		TESTRESULT(++number, "size", ,	(VecsRegistry().size<VeEntityTypeNode>() == 0), );

		TESTRESULT(++number, "erase handle", h2.erase(), (!h2.has_value() && VecsRegistry().size() == 0), );
		TESTRESULT(++number, "size", , (VecsRegistry().size<VeEntityTypeDraw>() == 0), );

		auto b = VecsRegistry().begin<VeComponentName>();
		auto e = VecsRegistry().end<VeComponentName>();

		int i = 0;
		bool test = true;

		for (auto&& [handle, name] : VecsRegistry().begin<VeComponentName>()) {
			if (!handle.is_valid()) continue;
			++i;
			if (name.m_name != "Node" && name.m_name != "Draw") { test = false; }
			//std::cout << "Entity " << name << "\n";
		}
		TESTRESULT(++number, "system create", , (test && i == 0), );

	}


	{
		const int num = 1000;

		for (int i = 0; i < num; i++) {
			auto h1 = VecsRegistry<VeEntityTypeNode>{}.insert(VeComponentName{ "Node" }, VeComponentPosition{}, VeComponentOrientation{}, VeComponentTransform{});
			auto h2 = VecsRegistry<VeEntityTypeDraw>{}.insert(VeComponentName{ "Draw" }, VeComponentPosition{}, VeComponentOrientation{}, VeComponentTransform{}, VeComponentMaterial{ 1 }, VeComponentGeometry{ 1 });
		}
		TESTRESULT(++number, "system create", , (VecsRegistry().size() == 2*num), );

		int i = 0;
		bool test = true;

		VecsRegistry().for_each<VeComponentName>([&](auto handle, auto& name) {
			++i;
			if (name.m_name != "Node" && name.m_name != "Draw") { test = false; }
			name.m_name = "Name Holder " + std::to_string(i);
			//std::cout << "Entity " << name.m_name << " " << i << "\n";
		});

		i = 0;
		VecsRegistry().for_each<VeComponentName>([&](auto handle, auto& name) {
			++i;
			if (name.m_name != "Name Holder " + std::to_string(i)) { test = false; }
			//std::cout << "Entity " << name.m_name << " " << i << "\n";
		});


		TESTRESULT(++number, "system run 1", , (test && i == 2 * num), );

		i = 0;
		test = true;
		for (auto&& [handle, name] : VecsRegistry().begin<VeComponentName>()) {
			VecsReadLock lock(handle.mutex());
			if (!handle.is_valid()) continue;
			++i;
			name.m_name = "Name Holder 2 " + std::to_string(i);
			//std::cout << "Entity " << name.m_name << " " << i << "\n";
		}

		i = 0;
		VecsRegistry().for_each<VeComponentName>([&](auto handle, auto& name) {
			++i;
			if (name.m_name != ("Name Holder 2 " + std::to_string(i))) { test = false; }
			//std::cout << "Entity " << name.m_name << " " << i << "\n";
		});

		TESTRESULT(++number, "system run 2", , test, );

		test = true;
		auto f = [&]<typename B, typename E>(B&& b, E&& e, auto& self, int& i) {
			if (b == e) return false;
			auto [handle, name] = *b;
			if (!handle.is_valid()) {
				return true;
			}

			//std::cout << "Entity IN " << name.m_name << " " << i << "\n";
			auto ii = i;
			b++;
			while (self(b, e, self, ++i)) { ++b;  --i; };

			if (name.m_name != ("Name Holder 2 " + std::to_string(ii))) { test = false; }
			//std::cout << "Entity " << name.m_name << " " << ii << "\n";
			name.m_name = "Name Holder 3 " + std::to_string(ii);
			return false;
		};

		auto b = VecsRegistry().begin<VeComponentName>();
		auto e = VecsRegistry().end<VeComponentName>();
		i = 1;
		while (f(b, e, f, i)) { b++; };

		TESTRESULT(++number, "system run 3", , test, );

		i = 0;
		test = true;
		VecsRegistry().for_each<VeComponentName>([&](auto handle, auto& name) {
			++i;
			if (name.m_name != ("Name Holder 3 " + std::to_string(i))) { test = false; }
			//std::cout << "Entity " << name.m_name << " " << i << "\n";
		});

		TESTRESULT(++number, "system run 4", , test, );

		TESTRESULT(++number, "clear", VecsRegistry().clear(), (VecsRegistry().size() == 0 && VecsRegistry().size<VeEntityTypeNode>()), );

		TESTRESULT(++number, "compress", VecsRegistry().compress(), true , );

	}


    return 0;
}

