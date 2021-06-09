
#include <iostream>
#include <utility>
#include "glm.hpp"
#include "gtc/quaternion.hpp"
#include "VECS.h"
#include "basic_test.h"
#include <ranges>

using namespace vecs;


void f( map_index_t idx) {
    return;
}


struct TData;

struct TIter {
    using value_type = int;
    using pointer = int*;
    using reference = int&;
    using difference_type = int;

    TData* m_ptr;
    size_t m_current{ 0 };

    TIter() = default;
    TIter(TData* ptr, size_t current) : m_ptr{ ptr }, m_current{ current } {};

    TIter(const TIter &v) = default;
    TIter(TIter&& v) = default;

    TIter& operator=(const TIter& v) = default;
    TIter& operator=(TIter&& v) = default;

    int& operator*();
    int& operator*() const;

    TIter& operator++() { ++m_current; return *this; }

    TIter operator++(int) {
        TIter tmp = *this;
        ++m_current;
        return tmp;
    };
    auto operator->() { return operator*(); };
    bool operator!=(const TIter& v) { return m_ptr != v.m_ptr || m_current != v.m_current; }
    friend bool operator==(const TIter& v1, const TIter& v2) { return v1.m_ptr == v2.m_ptr && v1.m_current == v2.m_current; };
};

struct TData : public std::ranges::view_base  {
    std::array<int, 100> m_data;
    size_t m_size{0};

    template<typename T>
    TData(T&& data) : m_size { data.max_size() } {
        std::copy_n(data.begin(), m_size, m_data.begin());
    };
    
    TIter begin() { return TIter(this, 0);  };
    TIter end() { return TIter(this, m_size); };

    int size() { return (int)m_size; };
};

int& TIter::operator*() { 
    return m_ptr->m_data[m_current]; 
};

int& TIter::operator*() const {
    return m_ptr->m_data[m_current];
};


int main() {

    bool sr = std::ranges::sized_range<TData>;

    TData data1(std::array<int, 9>{1, 2, 3, 4, 5, 6, 7, 8, 88});
    TData data2(std::array<int, 9>{1, 2, 3, 4, 5, 6, 7, 8, 88});

    for (auto&& i : data1) {
        std::cout << i << " ";
    }
    std::cout << "\n";

    auto it = std::begin(data1);
    const TIter& cit = it;

    int a = *it++;
    int b = *it++;
    *it = 99;
    int c = *cit;
   
    auto vdata1 = data1 | std::views::drop(0) | std::views::take(5);
    auto vdata2 = data2 | std::views::drop(5) | std::views::take(4);

    std::vector<decltype(vdata1)> tdata;
    tdata.push_back(vdata1);
    tdata.push_back(vdata2);

    auto joinedview = std::views::join(tdata);

    auto jvt = joinedview | std::views::take(6);

    for (auto&& i : jvt) {
        std::cout << i << " ";
    }
    std::cout << "\n";


    //-----------------------------------------------------

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
    auto comp1_3 = h1.component_ptr<MyComponentMaterial>();

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





