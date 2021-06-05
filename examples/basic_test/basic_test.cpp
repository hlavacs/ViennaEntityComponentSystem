
#include <iostream>
#include <utility>
#include "glm.hpp"
#include "gtc/quaternion.hpp"
#include "VECS.h"
#include "basic_test.h"

using namespace vecs;


void f( map_index_t idx) {
    return;
}


int main() {

    struct S {
        map_index_t map;
        table_index_t table;
    };

    std::atomic<map_index_t> aidx;
    std::atomic<S> str;
    std::atomic<VecsHandle> handle;

    map_index_t idx;

    f(idx);
    f(map_index_t{ 1 });
    //f(1);

    VecsRegistry reg;
    std::atomic_flag flag;

    std::cout << sizeof(VecsHandle) << " " << sizeof(flag) << std::endl;
    std::cout << vtll::size<MyEntityTypeList>::value << std::endl;

    //std::cout << typeid(vecs::VecsEntityTypeList).name() << std::endl << std::endl;
    //std::cout << typeid(vecs::expand_tags<VecsEntityTypeList>).name() << std::endl;

    auto h1 = VecsRegistry<MyEntityTypeNode>{}.insert(MyComponentName{ "Node" }, MyComponentPosition{ glm::vec3{9.0f, 2.0f, 3.0f} }, MyComponentOrientation{}, MyComponentTransform{});
    std::cout << typeid(MyEntityTypeNode).hash_code() << " " << typeid(MyEntityTypeNode).name() << std::endl;

    auto comp1_2 = h1.component<MyComponentPosition>();
    auto comp1_3 = h1.component<MyComponentMaterial>();

    //h1.update(MyComponentPosition{ glm::vec3{-9.0f, -2.0f, -3.0f} });
    h1.component<MyComponentPosition>() = MyComponentPosition{ glm::vec3{-9.0f, -2.0f, -3.0f} };
    auto comp1_4 = h1.component<MyComponentPosition>();

    //h1.update(MyComponentPosition{ glm::vec3{-999.0f, -2.0f, -3.0f} });
    h1.component<MyComponentPosition>() = MyComponentPosition{ glm::vec3{-999.0f, -2.0f, -3.0f} };
    auto comp1_5 = h1.component<MyComponentPosition>();

   VecsHandle h2{ VecsRegistry<MyEntityTypeDraw>{}.insert(MyComponentName{ "Draw" }, MyComponentPosition{ glm::vec3{9.0f, 2.0f, 3.0f} }, MyComponentOrientation{}, MyComponentTransform{}, MyComponentMaterial{ 99 }, MyComponentGeometry{}) };
    std::cout << typeid(MyEntityTypeDraw).hash_code() << " " << typeid(MyEntityTypeDraw).name() << std::endl;

    auto comp2_1 = h2.component<MyComponentMaterial>();
    auto comp2_2 = h2.component<MyComponentMaterial>();

    using entity_types = vtll::filter_have_all_types< MyEntityTypeList, vtll::type_list<MyComponentPosition> >;
    std::cout << typeid(entity_types).name() << std::endl;

    VecsRange<MyComponentPosition, MyComponentOrientation>{}.for_each([&](auto handle, auto& pos, auto& orient) {
        pos = MyComponentPosition{ glm::vec3{12345.0f, -299.0f, -334.0f} };
        std::cout << "entity\n";
    });

    auto comp1_6 = h1.component<MyComponentPosition>();

    h1.erase();
    h2.erase();

    return 0;
}

