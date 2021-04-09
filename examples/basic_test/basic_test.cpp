
#include <iostream>
#include <utility>
#include "glm.hpp"
#include "gtc/quaternion.hpp"

#include "basic_test.h"

#include "VECS.h"

using namespace vecs;

int main() {
    std::atomic_flag flag;
    
    std::cout << sizeof(VecsHandle) << " " << sizeof(flag) << std::endl;
    std::cout << vtll::size<VecsEntityTypeList>::value << std::endl;

    auto h1 = VecsRegistry<VeEntityTypeNode>{}.insert(VeComponentName{ "Node" }, VeComponentPosition{ glm::vec3{9.0f, 2.0f, 3.0f} }, VeComponentOrientation{}, VeComponentTransform{});
    std::cout << typeid(VeEntityTypeNode).hash_code() << " " << typeid(VeEntityTypeNode).name() << std::endl;

    auto data1b = h1.proxy<VeEntityTypeNode>();
    auto comp1_1 = data1b.component<VeComponentPosition>();

    auto comp1_2 = h1.component<VeComponentPosition>();
    auto comp1_3 = h1.component<VeComponentMaterial>();

    h1.update(VeComponentPosition{ glm::vec3{-9.0f, -2.0f, -3.0f} });
    auto comp1_4 = h1.component<VeComponentPosition>();

    data1b.update(VeComponentPosition{ glm::vec3{-999.0f, -2.0f, -3.0f} });
    data1b.update();
    auto comp1_5 = h1.component<VeComponentPosition>();

    VecsHandle h2{ VecsRegistry<VeEntityTypeDraw>{}.insert(VeComponentName{ "Draw" }, VeComponentPosition{ glm::vec3{9.0f, 2.0f, 3.0f} }, VeComponentOrientation{}, VeComponentTransform{}, VeComponentMaterial{ 99 }, VeComponentGeometry{}) };
    std::cout << typeid(VeEntityTypeDraw).hash_code() << " " << typeid(VeEntityTypeDraw).name() << std::endl;

    auto data2b = h2.proxy<VeEntityTypeDraw>();
    auto comp2_1 = data2b.component<VeComponentMaterial>();
    auto comp2_2 = h2.component<VeComponentMaterial>();

    using entity_types = vtll::filter_have_all_types< VecsEntityTypeList, vtll::type_list<VeComponentPosition> >;
    std::cout << typeid(entity_types).name() << std::endl;

    VecsRegistry().for_each<VeComponentPosition, VeComponentOrientation>( [&]( auto handle, auto& pos, auto& orient) {
        pos = VeComponentPosition{ glm::vec3{12345.0f, -299.0f, -334.0f} };
        std::cout << "entity\n";
    });

    auto comp1_6 = h1.component<VeComponentPosition>();

    h1.erase();
    auto data1c = h1.proxy<VeEntityTypeNode>();
    h2.erase();

    return 0;
}

