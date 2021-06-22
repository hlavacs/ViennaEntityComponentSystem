
#include <iostream>
#include <iomanip>
#include <utility>
#include <string>
#include <vector>

#include <algorithm>
#include <numeric>
#include <mutex>


#include "glm.hpp"
#include "gtc/quaternion.hpp"

#include "VECS.h"
#include "test.h"

#include <future>

using namespace vecs;
using namespace vecs::test;

#define TESTRESULT(N, S, EXPR, B, C) \
		EXPR; \
		std::cout << "Test " << std::right << std::setw(3) << N << "  " << std::left << std::setw(30) << S << " " << ( B ? "PASSED":"FAILED" ) << std::endl;\
		C;



int start_test() {
	int number = 0;
	std::atomic<int> counter = 0;

	VecsRegistry reg{};
	VecsRegistry<MyEntityTypeNode>{};
	VecsRegistry<MyEntityTypeDraw>{};

	MyComponentName name;
	MyComponentPosition pos{ glm::vec3{9.0f, 2.0f, 3.0f} };
	MyComponentPosition pos2{ glm::vec3{22.0f, 2.0f, 3.0f} };
	MyComponentPosition pos3{ glm::vec3{33.0f, 2.0f, 3.0f} };
	MyComponentPosition pos4{ glm::vec3{44.0f, 2.0f, 3.0f} };
	MyComponentOrientation orient{ glm::quat{glm::vec3{90.0f, 45.0f, 0.0f}} };
	//MyComponentTransform trans{ glm::mat4{ 1.0f } };
	MyComponentMaterial mat{ 99 };
	MyComponentGeometry geo{ 11 };

	std::cout << vtll::size<MyEntityTypeList>::value << std::endl;
	
	{
		auto range = VecsRange<MyComponentName>{};
		auto it = range.begin();

		for (auto [mutex, handle, name] : VecsRange<MyComponentName>{}) {
		}
		for (auto [mutex, handle, name] : VecsRange<>{}) {
		}

		//for (auto& [mutex, handle, transf] : VecsRange<MyComponentTransform>{}) {
		//}

		for (auto [mutex, handle, name, pos, orient, transf] : VecsRange<MyEntityTypeNode>{}) {
		}
	}

	{
		std::atomic<uint32_t> mutex = 0;

		VecsWriteLock::lock( &mutex );

		auto a1 = std::async([&]() {
			VecsWriteLock lock{ &mutex };
			TESTRESULT(++number, "locking thread", , (true), );
		});

		TESTRESULT(++number, "sleeping", , (true), );
		std::this_thread::sleep_for(1s);
		TESTRESULT(++number, "waking up", , (true), );

		VecsWriteLock::unlock(&mutex);

		TESTRESULT(++number, "unlocked", , (true), );
		a1.wait();
	}

	{
		TESTRESULT(++number, "size", , (VecsRegistry().size() == 0 && VecsRegistry<MyEntityTypeNode>().size() == 0 && VecsRegistry<MyEntityTypeDraw>().size() == 0), );

		TESTRESULT(++number, "insert",
			auto h0 = VecsRegistry<MyEntityTypeNode>{}.insert(MyComponentName{ "Node" }, pos, orient, MyComponentTransform{ glm::mat4{ 1.0f } }),
			h0.has_value() && VecsRegistry().size() == 1, );

		TESTRESULT(++number, "insert",
			auto h1 = VecsRegistry<MyEntityTypeNode>{}.insert(MyComponentName{ "Node" }, pos2, orient, MyComponentTransform{ glm::mat4{ 1.0f } }),
			h1.has_value() && VecsRegistry().size() == 2, );

		TESTRESULT(++number, "insert<type>",
			auto h2 = VecsRegistry<MyEntityTypeDraw>{}.insert(MyComponentName{ "Draw" }, pos, orient, mat, geo),
			h2.has_value() && VecsRegistry().size() == 3, );

		TESTRESULT(++number, "insert<type>",
			auto h3 = VecsRegistry<MyEntityTypeDraw>{}.insert(MyComponentName{ "Draw" }, pos, orient, mat, geo),
			h3.has_value() && VecsRegistry().size() == 4, );

		//--------------------------------------------------------------------------------------------------------------------------

		TESTRESULT(++number, "component handle", auto& comp1 = h0.component<MyComponentPosition>(),
			(comp1.m_position == glm::vec3{ 9.0f, 2.0f, 3.0f }), );

		TESTRESULT(++number, "component handle", auto bb1 = h0.has_component<MyComponentMaterial>(), (!bb1), );

		TESTRESULT(++number, "component handle", auto comp3 = h2.component<MyComponentMaterial>(), (comp3.i == 99), );

		TESTRESULT(++number, "value tuple", auto tup1 = VecsRegistry<MyEntityTypeNode>{}.tuple(h0),
			(std::get<MyComponentPosition&>(tup1).m_position == glm::vec3{ 9.0f, 2.0f, 3.0f }), );

		TESTRESULT(++number, "ptr tuple", auto tup2 = VecsRegistry<MyEntityTypeNode>{}.tuple_ptr(h1),
			(std::get<MyComponentPosition*>(tup2)->m_position == glm::vec3{ 22.0f, 2.0f, 3.0f }), );

		//--------------------------------------------------------------------------------------------------------------------------

		TESTRESULT(++number, "update", h0.update(MyComponentName{ "Node" }, MyComponentPosition{ glm::vec3{-99.0f, -22.0f, -33.0f} }),
			(h0.component<MyComponentPosition>().m_position == glm::vec3{ -99.0f, -22.0f, -33.0f }), );

		//std::get<MyComponentPosition>(tup1).m_position = glm::vec3{ -9.0f, -255.0f, -3.0f };
		//TESTRESULT(++number, "update tuple", VecsRegistry<MyEntityTypeNode>{}.update(h0, tup1),
		//	(h0.component<MyComponentPosition>().m_position == glm::vec3{ -9.0f, -255.0f, -3.0f }), );

		std::get<MyComponentPosition*>(tup2)->m_position = glm::vec3{ -9.0f, -255.0f, -355.0f };
		TESTRESULT(++number, "update tuple ref", , (h1.component<MyComponentPosition>().m_position == glm::vec3{ -9.0f, -255.0f, -355.0f }), );

		TESTRESULT(++number, "update handle", h0.update(MyComponentPosition{ glm::vec3{99.0f, 22.0f, 33.0f} }),
			(h0.component<MyComponentPosition>().m_position == glm::vec3{ 99.0f, 22.0f, 33.0f }), );

		TESTRESULT(++number, "update handle", h0.update(MyComponentPosition{ glm::vec3{-99.0f, -22.0f, -33.0f} }),
			(h0.component<MyComponentPosition>().m_position == glm::vec3{ -99.0f, -22.0f, -33.0f }), );

		TESTRESULT(++number, "update registry", VecsRegistry{}.update(h0, MyComponentPosition{ glm::vec3{-90.0f, -22.0f, -33.0f} }),
			(h0.component<MyComponentPosition>().m_position == glm::vec3{ -90.0f, -22.0f, -33.0f }), );

		TESTRESULT(++number, "update registry", VecsRegistry{}.update(h0, MyComponentName{ "Draw1" }, MyComponentPosition{ glm::vec3{-98.0f, -20.0f, -33.0f} }),
			(h0.component<MyComponentPosition>().m_position == glm::vec3{ -98.0f, -20.0f, -33.0f }), );

		TESTRESULT(++number, "update registry", VecsRegistry<MyEntityTypeNode>{}.update(h0, MyComponentName{ "Draw" }, MyComponentPosition{ glm::vec3{-97.0f, -22.0f, -33.0f} }),
			(h0.component<MyComponentPosition>().m_position == glm::vec3{ -97.0f, -22.0f, -33.0f }), );

		TESTRESULT(++number, "update registry", VecsRegistry<MyEntityTypeNode>{}.update<MyComponentPosition>(h0, MyComponentPosition{ glm::vec3{-97.0f, -22.0f, -30.0f} }),
			(h0.component<MyComponentPosition>().m_position == glm::vec3{ -97.0f, -22.0f, -30.0f }), );

		//--------------------------------------------------------------------------------------------------------------------------

		auto position1 = h0.component<MyComponentPosition>().m_position;
		auto position1_2 = h1.component<MyComponentPosition>().m_position;
		TESTRESULT(++number, "swap", auto swap1 = VecsRegistry{}.swap(h0, h1),
			(swap1	&& h0.component<MyComponentPosition>().m_position == position1
					&& h1.component<MyComponentPosition>().m_position == position1_2), );

		//--------------------------------------------------------------------------------------------------------------------------

		TESTRESULT(++number, "erase handle per entity", h3.erase(), (!h3.has_value() && VecsRegistry().size() == 3), );
		TESTRESULT(++number, "size", , (VecsRegistry<MyEntityTypeDraw>().size() == 1), );

		TESTRESULT(++number, "erase handle", h0.erase(), (!h0.has_value() && VecsRegistry().size() == 2), );
		TESTRESULT(++number, "size", , (VecsRegistry<MyEntityTypeNode>().size() == 1), );

		TESTRESULT(++number, "erase handle", h2.erase(), (!h2.has_value() && VecsRegistry().size() == 1), );
		TESTRESULT(++number, "size", , (VecsRegistry<MyEntityTypeDraw>().size() == 0), );

		TESTRESULT(++number, "clear", VecsRegistry{}.clear(), (!h1.has_value() && VecsRegistry().size() == 0 && VecsRegistry<MyEntityTypeNode>().size() == 0 && VecsRegistry<MyEntityTypeDraw>().size() == 0), );

		VecsRegistry{}.compress();

		int i = 0;
		bool test = true;

		auto range = VecsRange<MyComponentName>{};
		auto it = range.begin();

		for (auto [mutex, handle, name] : VecsRange<MyComponentName>{}) {
			VecsReadLock lock( mutex );
			if (!handle.is_valid()) continue;
			++i;
			if (name.m_name != "Node" && name.m_name != "Draw") { test = false; }
			//std::cout << "Entity " << name << "\n";
		}
		TESTRESULT(++number, "system create", , (test && i == 0), );
	}

	{
		const int num = 10;

		for (int i = 0; i < num; i++) {
			auto h1 = VecsRegistry<MyEntityTypeNode>{}.insert(MyComponentName{ "Node" }, MyComponentPosition{}, MyComponentOrientation{}, MyComponentTransform{});
			auto h2 = VecsRegistry<MyEntityTypeDraw>{}.insert(MyComponentName{ "Draw" }, MyComponentPosition{}, MyComponentOrientation{}, MyComponentMaterial{ 1 }, MyComponentGeometry{ 1 });
		}
		TESTRESULT(++number, "system create", , (VecsRegistry().size() == 2 * num && VecsRegistry<MyEntityTypeNode>().size() == num && VecsRegistry<MyEntityTypeNode>().size() == num), );

		VecsRange<MyComponentName> range0;
		auto it0 = range0.begin();
		auto res = *it0;

		int i = 0;
		bool test = true;
		for (auto [mutex, handle, name, pos, orient, trans] : VecsRange<MyEntityTypeNode>{}) {
			VecsReadLock lock( mutex );
			if (!handle.is_valid()) continue;
			++i;
			if (name.m_name != "Node" && name.m_name != "Draw") { test = false; }
		}

		TESTRESULT(++number, "system run 0", , (test&& i == num), );

		i = 0;
		VecsRange<MyComponentName>{}.for_each([&](auto& mutex, auto handle, auto& name) {
			++i;
			if (name.m_name != "Node" && name.m_name != "Draw") { 
				test = false; 
			}
			//std::cout << "Entity " << name.m_name << " " << i << "\n";
			name.m_name = "Name Holder 0 " + std::to_string(i);
			});

		i = 0;
		VecsRange<MyComponentName>{}.for_each([&](auto& mutex, auto handle, auto& name) {
			++i;
			if (name.m_name != "Name Holder 0 " + std::to_string(i)) { test = false; }
			//std::cout << "Entity " << name.m_name << " " << i << "\n";
			});


		TESTRESULT(++number, "system run 1", , (test&& i == 2 * num), );

		i = 0;
		test = true;
		for (auto [mutex, handle, name] : VecsRange<MyComponentName>{}) {
			VecsWriteLock lock( mutex );
			if (!handle.is_valid()) continue;
			++i;
			name.m_name = "Name Holder 2 " + std::to_string(i);
			//std::cout << "Entity " << name.m_name << " " << i << "\n";
		}

		i = 0;
		VecsRange<MyComponentName>{}.for_each([&](auto& mutex, auto handle, auto& name) {
			++i;
			if (name.m_name != ("Name Holder 2 " + std::to_string(i))) { 
				test = false; 
			}
			//std::cout << "Entity " << name.m_name << " " << i << "\n";
			});

		TESTRESULT(++number, "system run 2", , test, );

		i = 0;
		test = true;
		VecsRange<MyComponentName>{}.for_each([&](auto& mutex, auto handle, auto& name) {
			++i;
			if (name.m_name != ("Name Holder 2 " + std::to_string(i))) { test = false; }
			name.m_name = "Name Holder 4 " + std::to_string(i);
			//std::cout << "Entity " << name.m_name << " " << i << "\n";
			});

		TESTRESULT(++number, "system run 3", , test, );

		i = 0;
		VecsRange<MyComponentName>{}.for_each([&](auto& mutex, auto handle, auto& name) {
			++i;
			if (name.m_name != ("Name Holder 4 " + std::to_string(i))) { test = false; }
			//std::cout << "Entity " << name.m_name << " " << i << "\n";
			});

		TESTRESULT(++number, "system run 4", , test, );

		TESTRESULT(++number, "clear", VecsRegistry().clear(), (VecsRegistry().size() == 0 && VecsRegistry<MyEntityTypeNode>().size() == 0), );

		TESTRESULT(++number, "compress", VecsRegistry().compress(), true, );
	}


	{
		TESTRESULT(++number, "size", , (VecsRegistry().size() == 0 && VecsRegistry<MyEntityTypeNode>{}.size() == 0 && VecsRegistry<MyEntityTypeDraw>{}.size() == 0), );

		const int num = 767;
		const int L = 3;
		bool test = true;

		for (int l = 0; l < L; ++l) {
			for (int i = 0; i < num; ++i) {
				auto h1 = VecsRegistry<MyEntityTypeNode>{}.insert(MyComponentName{ "Node" }, pos, orient, MyComponentTransform{ glm::mat4{ 1.0f } });
				test = test && VecsRegistry<MyEntityTypeNode>{}.size() == i + 1;

				auto h2 = VecsRegistry<MyEntityTypeDraw>{}.insert(MyComponentName{ "Draw" }, pos, orient, MyComponentMaterial{ 1 }, MyComponentGeometry{ 1 });
				test = test && VecsRegistry<MyEntityTypeDraw>{}.size() == i + 1;
			}
			VecsRegistry{}.clear();
			test = test && VecsRegistry{}.size() == 0 && VecsRegistry<MyEntityTypeNode>{}.size()==0 && VecsRegistry<MyEntityTypeDraw>{}.size() == 0;
			VecsRegistry{}.compress();

		}
		TESTRESULT(++number, "system run 5", , test, );
	}

	{
		int i = 0;
		bool test = true;
		VecsRange<MyEntityTypeNode, MyEntityTypeDraw>{}.for_each([&](auto& mutex, auto handle, auto& name, auto& pos, auto& orient) {
			++i;
			if (name.m_name != ("Name Holder 4 " + std::to_string(i))) { test = false; }
			//std::cout << "Entity " << name.m_name << " " << i << "\n";
			});
		
		TESTRESULT(++number, "system run 6", , test, );

		TESTRESULT(++number, "clear", VecsRegistry{}.clear(), (VecsRegistry().size() == 0 && VecsRegistry<MyEntityTypeNode>().size() == 0 && VecsRegistry<MyEntityTypeDraw>().size() == 0), );
	}
	
	{
		const int num = 20000;
		bool flag = true;
		for (int i = 0; i < num; i++) {
			auto h1 = VecsRegistry<MyEntityTypeNode>{}.insert(MyComponentName{ "Node" }, MyComponentPosition{}, MyComponentOrientation{}, MyComponentTransform{});
			auto h2 = VecsRegistry<MyEntityTypeNodeTagged<TAG1>>{}.insert(MyComponentName{ "Node T1" }, MyComponentPosition{}, MyComponentOrientation{}, MyComponentTransform{});
			auto h3 = VecsRegistry<MyEntityTypeNodeTagged<TAG2>>{}.insert(MyComponentName{ "Node T2" }, MyComponentPosition{}, MyComponentOrientation{}, MyComponentTransform{});
			auto h4 = VecsRegistry<MyEntityTypeNodeTagged<TAG1,TAG2>>{}.insert(MyComponentName{ "Node T1+T2" }, MyComponentPosition{}, MyComponentOrientation{}, MyComponentTransform{});

			flag = flag && VecsRegistry{}.has_component<MyComponentName>(h1);
			flag = flag && VecsRegistry{}.has_component<TAG1>(h2);
			flag = flag && VecsRegistry{}.has_component<TAG2>(h3);
			flag = flag && VecsRegistry{}.has_component<TAG1>(h4) && VecsRegistry {}.has_component<TAG2>(h4);
		}

		TESTRESULT(++number, "tags", , (VecsRegistry().size() == 4 * num 
			&& VecsRegistry().size<MyEntityTypeNode>() == 4*num
			&& VecsRegistry<MyEntityTypeNode>().size() == num
			&& VecsRegistry<MyEntityTypeNodeTagged<TAG1>>().size() == num
			&& VecsRegistry<MyEntityTypeNodeTagged<TAG2>>().size() == num
			&& VecsRegistry<MyEntityTypeNodeTagged<TAG1,TAG2>>().size() == num
			&& flag
			), );

		VecsIterator<MyEntityTypeNode> b;
		VecsIterator<MyEntityTypeNode> e;
		VecsRange<MyEntityTypeNode> range;

		auto s = VecsRegistry<MyEntityTypeNodeTagged<TAG1>>().size();

		VecsIterator<MyEntityTypeNodeTagged<TAG1>> bt;
		VecsIterator<MyEntityTypeNodeTagged<TAG1>> et;
		VecsRange<MyEntityTypeNodeTagged<TAG1>> ranget;

		VecsRange<MyEntityTypeNode, TAG1>{}.for_each([&](auto& mutex, auto handle, auto& name, auto& pos, auto& orient, auto& transf) {
			//std::cout << handle.map_index().value << "\n";
		});

		VecsRange<MyEntityTypeNode, TAG1> range_par;
		auto split = range_par.split(2);

		split[0].for_each([&](auto& mutex, auto handle, auto& name, auto& pos, auto& orient, auto& transf) {
			//std::cout << handle.map_index().value << "\n";
			VecsRegistry<MyEntityTypeNode>{}.transform(handle);
			});

		split[1].for_each([&](auto& mutex, auto handle, auto& name, auto& pos, auto& orient, auto& transf) {
			//std::cout << handle.map_index().value << "\n";
			VecsRegistry<MyEntityTypeNode>{}.transform(handle);
			});

		VecsRange<MyEntityTypeNode, TAG1>{}.for_each([&](auto& mutex, auto handle, auto& name, auto& pos, auto& orient, auto& transf) {
			VecsRegistry<MyEntityTypeNode>{}.transform(handle);
		});

		TESTRESULT(++number, "tags", , (VecsRegistry().size() == 4 * num
			&& VecsRegistry().size<MyEntityTypeNode>() == 4 * num
			&& VecsRegistry<MyEntityTypeNode>().size() == 3*num
			&& VecsRegistry<MyEntityTypeNodeTagged<TAG1>>().size() == 0
			&& VecsRegistry<MyEntityTypeNodeTagged<TAG2>>().size() == num
			&& VecsRegistry<MyEntityTypeNodeTagged<TAG1, TAG2>>().size() == 0
			), );

		TESTRESULT(++number, "clear", VecsRegistry{}.clear(), (VecsRegistry().size() == 0 && VecsRegistry<MyEntityTypeNode>().size() == 0 && VecsRegistry<MyEntityTypeDraw>().size() == 0), );
	}

	{
		const int num = 1000;

		for (int i = 0; i < num; i++) {
			auto h1 = VecsRegistry<MyEntityTypeNode>{}.insert(MyComponentName{ "Node" }, MyComponentPosition{}, MyComponentOrientation{}, MyComponentTransform{});
			auto h2 = VecsRegistry<MyEntityTypeDraw>{}.insert(MyComponentName{ "Draw" }, MyComponentPosition{}, MyComponentOrientation{}, MyComponentMaterial{ 1 }, MyComponentGeometry{ 1 });
		}
		TESTRESULT(++number, "system create", , (VecsRegistry().size() == 2 * num && VecsRegistry<MyEntityTypeNode>().size() == num && VecsRegistry<MyEntityTypeNode>().size() == num), );

		int i = 0;

		VecsRange<MyEntityTypeNode, MyEntityTypeDraw>{}.for_each([&](auto& mutex, auto handle, auto& name, auto& pos, auto& orient) {
			orient.i = 1;
			++i;
		});
		TESTRESULT(++number, "summing", , (i == 2 * num), );

		int sum = 0;
		i = 0;
		for (auto [mutex, handle, orient] : VecsRange<MyComponentOrientation>{}) {
			if (!handle.is_valid()) continue;
			sum += orient.i;
			orient.i = orient.i * 2;
			++i;
		}
		TESTRESULT(++number, "summing", , (sum = 2*num && i == 2*num), );

		sum = 0;
		i = 0;
		for (auto [mutex, handle, name, pos, orient, transf] : VecsRange<MyEntityTypeNode>{}) {
			if (!handle.is_valid()) continue;
			sum += orient.i;
			handle.erase();
			++i;
		}
		TESTRESULT(++number, "summing", , (sum = num && i == num), );

		TESTRESULT(++number, "clear", VecsRegistry{}.clear(), (VecsRegistry().size() == 0 && VecsRegistry<MyEntityTypeNode>().size() == 0 && VecsRegistry<MyEntityTypeDraw>().size() == 0), );

		VecsRegistry{}.compress();
	}
	
	
	{
		for (int i = 0; i < 5; ++i) {
			const int num = 100000;
			auto a1 = std::async([&]() {
				for (int i = 0; i < num; i++) {
					auto h1 = VecsRegistry<MyEntityTypeNode>{}.insert(MyComponentName{ "Node" }, MyComponentPosition{}, MyComponentOrientation{}, MyComponentTransform{});
					auto h2 = VecsRegistry<MyEntityTypeDraw>{}.insert(MyComponentName{ "Draw" }, MyComponentPosition{}, MyComponentOrientation{}, MyComponentMaterial{ 1 }, MyComponentGeometry{ 1 });
				}
				});

			auto a2 = std::async([&]() {
				for (int i = 0; i < num; i++) {
					auto h1 = VecsRegistry<MyEntityTypeNode>{}.insert(MyComponentName{ "Node" }, MyComponentPosition{}, MyComponentOrientation{}, MyComponentTransform{});
					auto h2 = VecsRegistry<MyEntityTypeDraw>{}.insert(MyComponentName{ "Draw" }, MyComponentPosition{}, MyComponentOrientation{}, MyComponentMaterial{ 1 }, MyComponentGeometry{ 1 });
				}
			});

			auto a3 = std::async([&]() {
				for (int i = 0; i < num; i++) {
					auto h1 = VecsRegistry<MyEntityTypeNode>{}.insert(MyComponentName{ "Node" }, MyComponentPosition{}, MyComponentOrientation{}, MyComponentTransform{});
					auto h2 = VecsRegistry<MyEntityTypeDraw>{}.insert(MyComponentName{ "Draw" }, MyComponentPosition{}, MyComponentOrientation{}, MyComponentMaterial{ 1 }, MyComponentGeometry{ 1 });
				}
				});

			auto a4 = std::async([&]() {
				for (int i = 0; i < num; i++) {
					auto h1 = VecsRegistry<MyEntityTypeNode>{}.insert(MyComponentName{ "Node" }, MyComponentPosition{}, MyComponentOrientation{}, MyComponentTransform{});
					auto h2 = VecsRegistry<MyEntityTypeDraw>{}.insert(MyComponentName{ "Draw" }, MyComponentPosition{}, MyComponentOrientation{}, MyComponentMaterial{ 1 }, MyComponentGeometry{ 1 });
				}
				});

			a1.wait();
			a2.wait();
			a3.wait();
			a4.wait();

			int thr = 4;
			TESTRESULT(++number, "system create parallel", , (VecsRegistry().size() == 2*thr * num && VecsRegistry<MyEntityTypeNode>().size() == thr * num && VecsRegistry<MyEntityTypeNode>().size() == thr * num), );
			
			if (i < 4) {
				TESTRESULT(++number, "clear", VecsRegistry{}.clear(), (VecsRegistry().size() == 0 && VecsRegistry<MyEntityTypeNode>().size() == 0 && VecsRegistry<MyEntityTypeDraw>().size() == 0), );
				VecsRegistry{}.compress();
			}
		}
	}

	{
		for (int i = 0; i < 1; ++i) {
			bool sync = true;
			auto a1 = std::async([&]() {
				VecsRange<MyComponentOrientation, MyComponentTransform>{}.for_each([&](auto& mutex, auto handle, auto& orient, auto& transf) {
					orient.i = 11;
					transf.i = 11;
					}, sync);
				});

			auto a2 = std::async([&]() {
				VecsRange<MyComponentOrientation, MyComponentTransform>{}.for_each([&](auto& mutex, auto handle, auto& orient, auto& transf) {
					orient.i = 22;
					transf.i = 22;
					}, sync);
				});

			auto a3 = std::async([&]() {
				VecsRange<MyComponentOrientation, MyComponentTransform>{}.for_each([&](auto& mutex, auto handle, auto& orient, auto& transf) {
					orient.i = 33;
					transf.i = 33;
					}, sync);
				});

			auto a4 = std::async([&]() {
				VecsRange<MyComponentOrientation, MyComponentTransform>{}.for_each([&](auto& mutex, auto handle, auto& orient, auto& transf) {
					orient.i = 44;
					transf.i = 44;
					}, sync);
				});

			a1.wait();
			a2.wait();
			a3.wait();
			a4.wait();

			bool flag = true;
			for (auto [mutex, handle, orient, transf] : VecsRange<MyComponentOrientation, MyComponentTransform>{}) {
				if (!handle.is_valid()) continue;
				if (orient.i != transf.i) {
					flag = false;
				}
				if (orient.i != 11 && orient.i != 22 && orient.i != 33 && orient.i != 44) {
					flag = false;
				}
			}

			TESTRESULT(++number, "parallel update", , (flag), );
		}
	}


	{
		TESTRESULT(++number, "clear", VecsRegistry{}.clear(), (VecsRegistry().size() == 0 && VecsRegistry<MyEntityTypeNode>().size() == 0 && VecsRegistry<MyEntityTypeDraw>().size() == 0), );
		VecsRegistry{}.compress();
	}


    return 0;
}

int main() {

	for (int i = 0; i < 3; ++i) {
		start_test();
	}

}