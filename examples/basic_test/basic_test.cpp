
#include <iostream>
#include <utility>
#include "glm.hpp"
#include "gtc/quaternion.hpp"
#include "VECS.h"
#include "basic_test.h"

using namespace vecs;

int main() {
    using info = vtll::type_list<VecsHandle, index_t, index_t>;
    static const uint32_t c_handle = 0;
    static const uint32_t c_prev = 1;
    static const uint32_t c_next = 2;
    static const uint32_t c_info_size = 3;

    using types2 = vtll::cat< info, VeEntityTypeNode >;
    std::cout << typeid(types2).hash_code() << " " << typeid(types2).name() << std::endl;

    
    std::cout << sizeof(VecsHandle) << " " << sizeof(index_t) << std::endl;
    std::cout << vtll::size<VecsEntityTypeList>::value << std::endl;

    auto h1 = VecsRegistry{}.insert(VeComponentName{ "Node" }, VeComponentPosition{ glm::vec3{9.0f, 2.0f, 3.0f} }, VeComponentOrientation{}, VeComponentTransform{});
    std::cout << typeid(VeEntityTypeNode).hash_code() << " " << typeid(VeEntityTypeNode).name() << std::endl;

    auto data1b = h1.entity<VeEntityTypeNode>().value();
    auto comp1_1 = data1b.component<VeComponentPosition>();

    auto comp1_2 = h1.component<VeComponentPosition>();
    auto comp1_3 = h1.component<VeComponentMaterial>();

    h1.update(VeComponentPosition{ glm::vec3{-9.0f, -2.0f, -3.0f} });
    auto comp1_4 = h1.component<VeComponentPosition>();

    data1b.local_update(VeComponentPosition{ glm::vec3{-999.0f, -2.0f, -3.0f} });
    data1b.update();
    auto comp1_5 = h1.component<VeComponentPosition>();

    auto h2 = VecsRegistry{}.insert(VeComponentName{ "Draw" }, VeComponentMaterial{ 99 }, VeComponentGeometry{});
    std::cout << typeid(VeEntityTypeDraw).hash_code() << " " << typeid(VeEntityTypeDraw).name() << std::endl;

    auto data2b = h2.entity<VeEntityTypeDraw>().value();
    auto comp2_1 = data2b.component<VeComponentMaterial>();
    auto comp2_2 = h2.component<VeComponentMaterial>();

    using entity_types = vtll::filter_have_all_types< VecsEntityTypeList, vtll::type_list<VeComponentPosition> >;
    std::cout << typeid(entity_types).name() << std::endl;

    for_each<VeComponentPosition, VeComponentOrientation>( [&]( auto& iter) {

        auto [handle, pos, orient] = *iter;

        pos = VeComponentPosition{ glm::vec3{12345.0f, -299.0f, -334.0f} };

        std::cout << "entity\n";
    });

    auto comp1_6 = h1.component<VeComponentPosition>().value();

    h1.erase();
    auto data1c = h1.entity<VeEntityTypeNode>();
    h2.erase();

    return 0;
}

