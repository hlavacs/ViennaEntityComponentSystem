
#include <iostream>
#include <utility>
#include <ranges>
#include "glm.hpp"
#include "gtc/quaternion.hpp"
#include "VECS.h"
#include "basic_test.h"

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
    int m_current{ 0 };

    TIter() = default;
    TIter(TData* ptr, int current) : m_ptr{ ptr }, m_current{ current } {};

    TIter(const TIter &v) = default;
    TIter(TIter&& v) = default;

    TIter& operator=(const TIter& v) = default;
    TIter& operator=(TIter&& v) = default;

    int& operator*();
    int& operator*() const;

    TIter& operator++() { 
        ++m_current; 
        return *this; 
    }

    TIter operator++(int) {
        TIter tmp = *this;
        ++m_current;
        return tmp;
    };

    TIter& operator+=( int n ) { 
        m_current = m_current + n; 
        return *this; 
    }

    TIter operator+(int n) { 
        return TIter(m_ptr, m_current + n); 
    }

    auto operator->() { return operator*(); };
    bool operator!=(const TIter& v) { return m_ptr != v.m_ptr || m_current != v.m_current; }
    friend bool operator==(const TIter& v1, const TIter& v2) { return v1.m_ptr == v2.m_ptr && v1.m_current == v2.m_current; };
};

struct TData : public std::ranges::view_base  {
    std::array<int, 100> m_data;
    int m_size{0};

    template<typename T>
    TData(T&& data) : m_size { (int)data.max_size() } {
        std::copy_n(data.begin(), m_size, m_data.begin());
    };
    
    TIter begin() { return TIter(this, 0);  };
    TIter end() { return TIter(this, m_size); };

    int size() { return m_size; };
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
    TData data2(std::array<int, 9>{11, 21, 31, 41, 51, 61, 71, 81, 881});

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
   
    auto vdata1 = data1 | std::views::drop(0) | std::views::take(9);
    auto vdata2 = data2 | std::views::drop(0) | std::views::take(9);

    std::vector<decltype(vdata1)> tdata;
    tdata.push_back(vdata1);
    tdata.push_back(vdata2);

    auto joinedview = std::views::join(tdata);

    auto jvt = joinedview | std::views::drop(5) | std::views::take(10);

    for (auto&& i : jvt) {
        std::cout << i << " ";
    }
    std::cout << "\n\n";

    auto r1 = joinedview | std::views::drop(0) | std::views::take(18);
    auto r2 = r1 | std::views::drop(6) | std::views::take(12);
    auto r3 = r2 | std::views::drop(6) | std::views::take(6);

    for (auto&& i : r1 | std::views::take(6)) {
        std::cout << i << " ";
    }
    std::cout << "\n";
    for (auto&& i : r2 | std::views::take(6)) {
        std::cout << i << " ";
    }
    std::cout << "\n";
    for (auto&& i : r3 | std::views::take(6)) {
        std::cout << i << " ";
    }
    std::cout << "\n\n";

    //----------------------------------------------------
    auto sr1b = data1.begin();
    auto sr1e = sr1b + 6;
    auto sr1 = std::ranges::subrange(sr1b, sr1e) | std::views::all;

    auto sr2b1 = sr1e;
    auto sr2e1 = data1.end();
    auto sr21 = std::ranges::subrange(sr2b1, sr2e1) | std::views::all;
    std::cout << typeid(sr21).name() << "\n";
    std::vector<std::ranges::subrange<struct TIter, struct TIter>> sr2v;
    sr2v.push_back(sr21);

    auto sr2b2 = data2.begin();
    auto sr2e2 = sr2b2 + 3;
    auto sr22 = std::ranges::subrange(sr2b2, sr2e2) | std::views::all;
    sr2v.push_back(sr22);

    auto range = std::views::join(sr2v);
    std::cout << typeid(range).name() << "\n";

    auto sr3b = sr2e2;
    auto sr3e = data2.end();
    auto sr3 = std::ranges::subrange(sr3b, sr3e) | std::views::all;

    for (auto&& i : sr1) {
        std::cout << i << " ";
    }
    std::cout << "\n";
    for (auto&& i : std::views::join(sr2v)) {
        std::cout << i << " ";
    }
    std::cout << "\n";
    for (auto&& i : sr3) {
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





