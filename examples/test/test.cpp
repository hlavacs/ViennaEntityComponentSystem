
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

	VecsRegistry reg{};
	VecsRegistry<VeEntityTypeNode>{};
	VecsRegistry<VeEntityTypeDraw>{};

	VeComponentName name;
	VeComponentPosition pos{ glm::vec3{9.0f, 2.0f, 3.0f} };
	VeComponentPosition pos2{ glm::vec3{22.0f, 2.0f, 3.0f} };
	VeComponentPosition pos3{ glm::vec3{33.0f, 2.0f, 3.0f} };
	VeComponentPosition pos4{ glm::vec3{44.0f, 2.0f, 3.0f} };
	VeComponentOrientation orient{ glm::quat{glm::vec3{90.0f, 45.0f, 0.0f}} };
	VeComponentTransform trans{ glm::mat4{ 1.0f } };
	VeComponentMaterial mat{ 99 };
	VeComponentGeometry geo{ 11 };


	{
		auto range = VecsRange<VeComponentName>{};
		auto it = range.begin();
		auto res = *it;

		for (auto&& [handle, name] : VecsRange<VeComponentName>{}) {
		}
		for (auto&& [handle, name] : VecsRange<>{}) {
		}
		for (auto&& [handle, name, pos, orient, transf] : VecsRange<VeEntityTypeNode>{}) {
		}

	}

	{
		TESTRESULT(++number, "size", , (VecsRegistry().size() == 0 && VecsRegistry<VeEntityTypeNode>().size() == 0 && VecsRegistry<VeEntityTypeDraw>().size() == 0), );

		TESTRESULT(++number, "insert",
			auto h1 = VecsRegistry<VeEntityTypeNode>{}.insert(VeComponentName{ "Node" }, pos, orient, trans),
			h1.has_value() && VecsRegistry().size() == 1, );

		TESTRESULT(++number, "insert",
			auto h1_2 = VecsRegistry<VeEntityTypeNode>{}.insert(VeComponentName{ "Node" }, pos2, orient, trans),
			h1_2.has_value() && VecsRegistry().size() == 2, );

		TESTRESULT(++number, "insert<type>",
			auto h2 = VecsRegistry<VeEntityTypeDraw>{}.insert(VeComponentName{ "Draw" }, pos, orient, mat, geo),
			h2.has_value() && VecsRegistry().size() == 3, );

		TESTRESULT(++number, "insert<type>",
			auto h3 = VecsRegistry<VeEntityTypeDraw>{}.insert(VeComponentName{ "Draw" }, pos, orient, mat, geo),
			h3.has_value() && VecsRegistry().size() == 4, );

		//--------------------------------------------------------------------------------------------------------------------------

		TESTRESULT(++number, "component handle", auto comp1 = h1.component<VeComponentPosition>(),
			(comp1.m_position == glm::vec3{ 9.0f, 2.0f, 3.0f }), );

		TESTRESULT(++number, "component handle", auto bb1 = h1.has_component<VeComponentMaterial>(), (!bb1), );

		TESTRESULT(++number, "component handle", auto comp3 = h2.component<VeComponentMaterial>(), (comp3.i == 99), );

		TESTRESULT(++number, "value tuple", auto tup1 = VecsRegistry<VeEntityTypeNode>{}.values(h1),
			(get<VeComponentPosition>(tup1).m_position == glm::vec3{ 9.0f, 2.0f, 3.0f }), );

		TESTRESULT(++number, "ptr tuple", auto tup2 = VecsRegistry<VeEntityTypeNode>{}.pointers(h1_2),
			(get<VeComponentPosition*>(tup2)->m_position == glm::vec3{ 22.0f, 2.0f, 3.0f }), );

		//--------------------------------------------------------------------------------------------------------------------------

		TESTRESULT(++number, "update", h1.update(VeComponentName{ "Node" }, VeComponentPosition{ glm::vec3{-99.0f, -22.0f, -33.0f} }),
			(h1.component<VeComponentPosition>().m_position == glm::vec3{ -99.0f, -22.0f, -33.0f }), );

		std::get<VeComponentPosition>(tup1).m_position = glm::vec3{ -9.0f, -255.0f, -3.0f };
		TESTRESULT(++number, "update tuple", VecsRegistry<VeEntityTypeNode>{}.update(h1, tup1),
			(h1.component<VeComponentPosition>().m_position == glm::vec3{ -9.0f, -255.0f, -3.0f }), );

		std::get<VeComponentPosition*>(tup2)->m_position = glm::vec3{ -9.0f, -255.0f, -355.0f };
		TESTRESULT(++number, "update tuple ref", , (h1_2.component<VeComponentPosition>().m_position == glm::vec3{ -9.0f, -255.0f, -355.0f }), );

		TESTRESULT(++number, "update handle", h1.update(VeComponentPosition{ glm::vec3{99.0f, 22.0f, 33.0f} }),
			(h1.component<VeComponentPosition>().m_position == glm::vec3{ 99.0f, 22.0f, 33.0f }), );

		TESTRESULT(++number, "update handle", h1.update(VeComponentPosition{ glm::vec3{-99.0f, -22.0f, -33.0f} }),
			(h1.component<VeComponentPosition>().m_position == glm::vec3{ -99.0f, -22.0f, -33.0f }), );

		TESTRESULT(++number, "update registry", VecsRegistry{}.update(h1, VeComponentPosition{ glm::vec3{-90.0f, -22.0f, -33.0f} }),
			(h1.component<VeComponentPosition>().m_position == glm::vec3{ -90.0f, -22.0f, -33.0f }), );

		TESTRESULT(++number, "update registry", VecsRegistry{}.update(h1, VeComponentName{ "Draw" }, VeComponentPosition{ glm::vec3{-98.0f, -20.0f, -33.0f} }),
			(h1.component<VeComponentPosition>().m_position == glm::vec3{ -98.0f, -20.0f, -33.0f }), );

		TESTRESULT(++number, "update registry", VecsRegistry<VeEntityTypeNode>{}.update(h1, VeComponentPosition{ glm::vec3{-97.0f, -22.0f, -33.0f} }),
			(h1.component<VeComponentPosition>().m_position == glm::vec3{ -97.0f, -22.0f, -33.0f }), );

		TESTRESULT(++number, "update registry", VecsRegistry<VeEntityTypeNode>{}.update<VeComponentPosition>(h1, VeComponentPosition{ glm::vec3{-97.0f, -22.0f, -30.0f} }),
			(h1.component<VeComponentPosition>().m_position == glm::vec3{ -97.0f, -22.0f, -30.0f }), );

		//--------------------------------------------------------------------------------------------------------------------------

		auto position1 = h1.component<VeComponentPosition>().m_position;
		auto position1_2 = h1_2.component<VeComponentPosition>().m_position;
		TESTRESULT(++number, "swap", auto swap1 = VecsRegistry{}.swap(h1, h1_2),
			(swap1	&& h1.component<VeComponentPosition>().m_position == position1
					&& h1_2.component<VeComponentPosition>().m_position == position1_2), );

		//--------------------------------------------------------------------------------------------------------------------------

		TESTRESULT(++number, "erase handle per entity", h3.erase(), (!h3.has_value() && VecsRegistry().size() == 3), );
		TESTRESULT(++number, "size", , (VecsRegistry<VeEntityTypeDraw>().size() == 1), );

		TESTRESULT(++number, "erase handle", h1.erase(), (!h1.has_value() && VecsRegistry().size() == 2), );
		TESTRESULT(++number, "size", , (VecsRegistry<VeEntityTypeNode>().size() == 1), );

		TESTRESULT(++number, "erase handle", h2.erase(), (!h2.has_value() && VecsRegistry().size() == 1), );
		TESTRESULT(++number, "size", , (VecsRegistry<VeEntityTypeDraw>().size() == 0), );

		TESTRESULT(++number, "clear", VecsRegistry{}.clear(), (!h1_2.has_value() && VecsRegistry().size() == 0 && VecsRegistry<VeEntityTypeNode>().size() == 0 && VecsRegistry<VeEntityTypeDraw>().size() == 0), );

		int i = 0;
		bool test = true;

		auto range = VecsRange<VeComponentName>{};
		auto it = range.begin();
		auto res = *it;

		for (auto&& [handle, name] : VecsRange<VeComponentName>{}) {
			VecsReadLock lock( handle.mutex() );
			if (!handle.has_value()) continue;
			++i;
			if (name.m_name != "Node" && name.m_name != "Draw") { test = false; }
			//std::cout << "Entity " << name << "\n";
		}
		TESTRESULT(++number, "system create", , (test && i == 0), );
	}


	{
		const int num = 100;

		for (int i = 0; i < num; i++) {
			auto h1 = VecsRegistry<VeEntityTypeNode>{}.insert(VeComponentName{ "Node" }, VeComponentPosition{}, VeComponentOrientation{}, VeComponentTransform{});
			auto h2 = VecsRegistry<VeEntityTypeDraw>{}.insert(VeComponentName{ "Draw" }, VeComponentPosition{}, VeComponentOrientation{}, VeComponentMaterial{ 1 }, VeComponentGeometry{ 1 });
		}
		TESTRESULT(++number, "system create", , (VecsRegistry().size() == 2 * num && VecsRegistry<VeEntityTypeNode>().size() == num && VecsRegistry<VeEntityTypeNode>().size() == num), );

		VecsRange<VeComponentName> range0;
		auto it0 = range0.begin();
		auto res = *it0;

		int i = 0;
		bool test = true;
		for (auto&& [handle, name, pos, orient, trans] : VecsRange<VeEntityTypeNode>{}) {
			VecsReadLock lock(handle.mutex());
			if (!handle.has_value()) continue;
			++i;
			if (name.m_name != "Node" && name.m_name != "Draw") { test = false; }
		}

		i = 0;
		VecsRegistry().for_each<VeComponentName>([&](auto handle, auto& name) {
			++i;
			if (name.m_name != "Node" && name.m_name != "Draw") { 
				test = false; 
			}
			//std::cout << "Entity " << name.m_name << " " << i << "\n";
			name.m_name = "Name Holder " + std::to_string(i);
			});

		i = 0;
		VecsRegistry().for_each<VeComponentName>([&](auto handle, auto& name) {
			++i;
			if (name.m_name != "Name Holder " + std::to_string(i)) { test = false; }
			//std::cout << "Entity " << name.m_name << " " << i << "\n";
			});


		TESTRESULT(++number, "system run 1", , (test&& i == 2 * num), );

		i = 0;
		test = true;
		for (auto&& [handle, name] : VecsRange<VeComponentName>{}) {
			VecsReadLock lock(handle.mutex());
			if (!handle.has_value()) continue;
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

		VecsRange<VeComponentName> range;
		auto b = range.begin();
		auto e = range.end();
		i = 1;
		while (f(b, e, f, i)) { b++; };

		i = 0;
		test = true;
		VecsRegistry().for_each<VeComponentName>([&](auto handle, auto& name) {
			++i;
			if (name.m_name != ("Name Holder 3 " + std::to_string(i))) { test = false; }
			name.m_name = "Name Holder 4 " + std::to_string(i);
			//std::cout << "Entity " << name.m_name << " " << i << "\n";
			});

		TESTRESULT(++number, "system run 3", , test, );


		i = 0;
		VecsRegistry().for_each<VeComponentName>([&](auto handle, auto& name) {
			++i;
			if (name.m_name != ("Name Holder 4 " + std::to_string(i))) { test = false; }
			//std::cout << "Entity " << name.m_name << " " << i << "\n";
			});

		TESTRESULT(++number, "system run 4", , test, );

		TESTRESULT(++number, "clear", VecsRegistry().clear(), (VecsRegistry().size() == 0 && VecsRegistry<VeEntityTypeNode>().size() == 0), );

		TESTRESULT(++number, "compress", VecsRegistry().compress(), true, );

	}


	{
		TESTRESULT(++number, "size", , (VecsRegistry().size() == 0 && VecsRegistry<VeEntityTypeNode>{}.size() == 0 && VecsRegistry<VeEntityTypeDraw>{}.size() == 0), );

		const int num = 767;
		const int L = 3;
		bool test = true;

		for (int l = 0; l < L; ++l) {
			for (int i = 0; i < num; ++i) {
				auto h1 = VecsRegistry<VeEntityTypeNode>{}.insert(VeComponentName{ "Node" }, pos, orient, trans);
				test = test && VecsRegistry<VeEntityTypeNode>{}.size() == i + 1;

				auto h2 = VecsRegistry<VeEntityTypeDraw>{}.insert(VeComponentName{ "Draw" }, pos, orient, VeComponentMaterial{ 1 }, VeComponentGeometry{ 1 });
				test = test && VecsRegistry<VeEntityTypeDraw>{}.size() == i + 1;
			}
			VecsRegistry{}.clear();
			test = test && VecsRegistry{}.size() == 0 && VecsRegistry<VeEntityTypeNode>{}.size()==0 && VecsRegistry<VeEntityTypeDraw>{}.size() == 0;
			VecsRegistry{}.compress();

		}
		TESTRESULT(++number, "system run 5", , test, );
	}

	{
		int i = 0;
		bool test = true;
		VecsRegistry().for_each<VeEntityTypeNode, VeEntityTypeDraw>( [&](auto handle, auto name, auto pos, auto orient) {
			++i;
			if (name.m_name != ("Name Holder 4 " + std::to_string(i))) { test = false; }
			//std::cout << "Entity " << name.m_name << " " << i << "\n";
			});
		
		TESTRESULT(++number, "system run 6", , test, );
	}
	
	{
		TESTRESULT(++number, "clear", VecsRegistry{}.clear(), VecsRegistry{}.size() == 0, );
		//VecsRegistry{}.compress();

		//std::cout << typeid(vecs::VecsEntityTypeList).name() << std::endl << std::endl;

		const int num = 10;
		bool flag = true;
		for (int i = 0; i < num; i++) {
			auto h1 = VecsRegistry<VeEntityTypeNode>{}.insert(VeComponentName{ "Node" }, VeComponentPosition{}, VeComponentOrientation{}, VeComponentTransform{});
			auto h2 = VecsRegistry<VeEntityTypeNodeTagged<TAG1>>{}.insert(VeComponentName{ "Node T1" }, VeComponentPosition{}, VeComponentOrientation{}, VeComponentTransform{});
			auto h3 = VecsRegistry<VeEntityTypeNodeTagged<TAG2>>{}.insert(VeComponentName{ "Node T2" }, VeComponentPosition{}, VeComponentOrientation{}, VeComponentTransform{});
			auto h4 = VecsRegistry<VeEntityTypeNodeTagged<TAG1,TAG2>>{}.insert(VeComponentName{ "Node T1+T2" }, VeComponentPosition{}, VeComponentOrientation{}, VeComponentTransform{});

			flag = flag && VecsRegistry{}.has_component<VeComponentName>(h1);
			flag = flag && VecsRegistry{}.has_component<TAG1>(h2);
			flag = flag && VecsRegistry{}.has_component<TAG2>(h3);
			flag = flag && VecsRegistry{}.has_component<TAG1>(h4) && VecsRegistry {}.has_component<TAG2>(h4);
		}

		TESTRESULT(++number, "tags", , (VecsRegistry().size() == 4 * num 
			&& VecsRegistry().size<VeEntityTypeNode>() == 4*num
			&& VecsRegistry<VeEntityTypeNode>().size() == num
			&& VecsRegistry<VeEntityTypeNodeTagged<TAG1>>().size() == num
			&& VecsRegistry<VeEntityTypeNodeTagged<TAG2>>().size() == num
			&& VecsRegistry<VeEntityTypeNodeTagged<TAG1,TAG2>>().size() == num
			&& flag
			), );


		VecsIterator<VeEntityTypeNode> b;
		VecsIterator<VeEntityTypeNode> e( true );
		VecsRange<VeEntityTypeNode> range;

		auto s = VecsRegistry<VeEntityTypeNodeTagged<TAG1>>().size();

		VecsIterator<VeEntityTypeNodeTagged<TAG1>> bt;
		VecsIterator<VeEntityTypeNodeTagged<TAG1>> et(true);
		VecsRange<VeEntityTypeNodeTagged<TAG1>> ranget;

		//VecsRegistry().for_each<VeEntityTypeNode, TAG1>([&](auto handle, auto& name, auto& pos, auto& orient, auto& transf) {
		//	VecsRegistry<VeEntityTypeNode>{}.transform(handle);
		//);

		VecsRange<VeEntityTypeNode> range_par;
		auto split = range_par.split(2);

		VecsRegistry().for_each( std::move(split[0]), [&](auto handle, auto& name, auto& pos, auto& orient, auto& transf) {
			VecsRegistry<VeEntityTypeNode>{}.transform(handle);
		});
		VecsRegistry().for_each(std::move(split[1]), [&](auto handle, auto& name, auto& pos, auto& orient, auto& transf) {
			VecsRegistry<VeEntityTypeNode>{}.transform(handle);
		});

		TESTRESULT(++number, "tags", , (VecsRegistry().size() == 4 * num
			&& VecsRegistry().size<VeEntityTypeNode>() == 4 * num
			&& VecsRegistry<VeEntityTypeNode>().size() == 3*num
			&& VecsRegistry<VeEntityTypeNodeTagged<TAG1>>().size() == 0
			&& VecsRegistry<VeEntityTypeNodeTagged<TAG2>>().size() == num
			&& VecsRegistry<VeEntityTypeNodeTagged<TAG1, TAG2>>().size() == 0
			), );


	}


    return 0;
}

