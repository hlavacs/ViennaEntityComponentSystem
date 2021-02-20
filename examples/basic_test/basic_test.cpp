
#include <iostream>
#include <utility>
#include "glm.hpp"
#include "gtc/quaternion.hpp"
#include "VECS.h"
#include "basic_test.h"

using namespace vecs;

int main() {
    
    VeEntityTable et;

    std::cout << sizeof(VeHandle) << " " << sizeof(index_t) << std::endl;
    std::cout << vtll::size<VeEntityTypeList>::value << std::endl;

    VeHandle h1 = et.insert(VeComponentPosition{ glm::vec3{9.0f, 2.0f, 3.0f} }, VeComponentOrientation{}, VeComponentTransform{});
    std::cout << typeid(VeEntityNode).hash_code() << " " << typeid(VeEntityNode).name() << std::endl;

    auto data1  = et.entity(h1);
    auto data1b = et.entity<VeEntityNode>(h1).value();
    auto comp1_1 = data1b.component<VeComponentPosition>();

    auto comp1_2 = et.component<VeComponentPosition>(h1);
    auto comp1_3 = et.component<VeComponentMaterial>(h1);

    et.update<VeComponentPosition>(h1, VeComponentPosition{ glm::vec3{-9.0f, -2.0f, -3.0f} });
    auto comp1_4 = et.component<VeComponentPosition>(h1);

    data1b.update<VeComponentPosition>(VeComponentPosition{ glm::vec3{-999.0f, -2.0f, -3.0f} });
    et.update(h1, data1b);
    auto comp1_5 = et.component<VeComponentPosition>(h1);

    VeHandle h2 = et.insert(VeComponentMaterial{ 99 }, VeComponentGeometry{});
    std::cout << typeid(VeEntityDraw).hash_code() << " " << typeid(VeEntityDraw).name() << std::endl;

    auto data2 = et.entity(h2);
    auto data2b = et.entity<VeEntityDraw>(h2);
    auto comp2_1 = data2b.value().component<VeComponentMaterial>();
    auto comp2_2 = et.component<VeComponentMaterial>(h2);

    using entity_types = typename vtll::filter_have_all_types< VeEntityTypeList, vtll::type_list<VeComponentPosition> >::type;
    std::cout << typeid(entity_types).name() << std::endl;

    for_each<VeComponentPosition, VeComponentOrientation>( [&]( auto& iter) {

        auto [handle, pos, orient] = *iter;

        pos = VeComponentPosition{ glm::vec3{12345.0f, -299.0f, -334.0f} };

        std::cout << "entity\n";
    });

    auto comp1_6 = et.component<VeComponentPosition>(h1).value();

    et.erase(h1);
    auto data1c = et.entity<VeEntityNode>(h1);
    et.erase(h2);

    return 0;
}

