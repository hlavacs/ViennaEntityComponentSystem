#define REGISTRYTYPE_PARALLEL

#include <iostream>
#include <string>
#include <gtest/gtest.h>

#include "VECSManager.h"
#include "test_helper.cpp"


class ManagerTest : public testing::Test {
    protected:
        ManagerTest(){}

        vecs::Manager mng{};
};


TEST_F(ManagerTest, CreateAndChangeEntityWorks) {
    vecs::Handle h1 = mng.Insert(5, 7.6f, 3.2);

    ASSERT_EQ(mng.Get<int>(h1), 5);
    ASSERT_EQ(mng.Get<float>(h1), 7.6f);
    ASSERT_EQ(mng.Get<double>(h1), 3.2);

    auto [c1, c2] = mng.Get<int&, double&>(h1);
    c1 = 7;
    c2 = 1.8;

    ASSERT_EQ(mng.Get<int>(h1), 7);
    ASSERT_EQ(mng.Get<double>(h1), 1.8);
}


TEST_F(ManagerTest, PutWorks) {
    vecs::Handle h2 = mng.Insert(5);
    mng.Put(h2, 6.4);

    ASSERT_EQ(mng.Get<double>(h2), 6.4);

    std::tuple<float, std::string> tup = {1.2f, "hi"};
    mng.Put(h2, tup);

    ASSERT_EQ(mng.Get<float>(h2), 1.2f);
    ASSERT_EQ(mng.Get<std::string>(h2), "hi");
}



TEST_F(ManagerTest, AddTagsWorks) {
    vecs::Handle h3 = mng.Insert(4, 5.5, 6.6f);
    vecs::Handle h4 = mng.Insert(9, 8.8, 7.7f);
    vecs::Handle h5 = mng.Insert(7, 6.6, 5.5f);

    mng.AddTags(h3, 1ul, 3ul);
    mng.AddTags(h4, 2ul, 3ul, 1ul);
    mng.AddTags(h5, 1ul, 2ul);

    int yesTagsCorrect = 0;
    int yesNoTagsCorrect = 0;

    auto yesTagsView = mng.template GetView<vecs::Handle>(std::vector<size_t>{1ul});
    auto yesNoTagsView = mng.template GetView<vecs::Handle>(std::vector<size_t>{2ul}, std::vector<size_t>{3ul});

    for ( auto handle : yesTagsView ) {
        //expect h3, h4, h5
        if (handle == h3 || handle == h4 || handle == h5) yesTagsCorrect += 1;
    }

    for ( auto handle : yesNoTagsView ) {
        //expect h5
        if (handle == h5) yesNoTagsCorrect += 1;
    }

    ASSERT_EQ(yesTagsCorrect, 3);
    ASSERT_EQ(yesNoTagsCorrect, 1);
}


TEST_F(ManagerTest, EraseTagsWorks) {
    vecs::Handle h6 = mng.Insert(4, 5.5, 6.6f);

    mng.AddTags(h6, 4ul, 5ul, 6ul);

    bool tagErased = true;
    mng.EraseTags(h6, 6ul);

    auto res = mng.template GetView<vecs::Handle>(std::vector<size_t>{6ul});
    for ( auto handle : res)  {
        if (handle == h6) tagErased = false;
    }

    ASSERT_TRUE(tagErased);
}


TEST_F(ManagerTest, EraseComponentAndEntityWorks) {
    vecs::Handle h7 = mng.Insert(5);
    vecs::Handle h8 = mng.Insert(4, 5.5f);

    mng.Erase(h7);
    mng.Erase<int>(h8);

    bool noInt = true;
    auto res = mng.template GetView<vecs::Handle, int>();

    for ( auto [handle, i] : res ) {
        if (handle == h7 || handle == h8) noInt = false;
    }

    ASSERT_TRUE(noInt);
}


TEST_F(ManagerTest, ClearWorks) {
    vecs::Handle h9 = mng.Insert(5);
    vecs::Handle h10 = mng.Insert(4, 5.5f);

    int size = 0;
    auto resa = mng.GetView<vecs::Handle>();
    
    for ( auto handle : resa) {
        size += 1;
    }

    ASSERT_EQ(size, 2);

    mng.Clear();
    size = 0;
    auto resb = mng.GetView<vecs::Handle>();

    for ( auto handle : resb ) {
        size += 1;
    }

    ASSERT_EQ(size, 0);
}


TEST_F(ManagerTest, HasAllWorks) {
    fillRegistryBasic(mng);

    auto threeCompView = mng.GetView<vecs::Handle, int, double, float>();
    auto twoCompView = mng.GetView<vecs::Handle, int, double>();

    // num of entities with components int, double and float
    size_t countThreeComp = 0;
    // num of entities with components int and double
    size_t countTwoComp = 0;

    for(auto [handle, i, d, f] : threeCompView) {
        ASSERT_TRUE(mng.HasAll(handle, i, d, f));
        countThreeComp += 1;
        ASSERT_TRUE(mng.HasAll(handle, i, d));
        countTwoComp += 1;
    }

    std::cout << "threeCompView has counted:\n    " << countThreeComp << " entities with components int, double and float.\n"
                << "    " << countTwoComp << " entities with components int and double.\n";

    countTwoComp = 0;
    for(auto [handle, i, d] : twoCompView) {
        ASSERT_TRUE(mng.HasAll(handle, i, d));
        countTwoComp += 1;
    }

    std::cout << "twoCompView has counted:\n    " << countTwoComp << " entities with components int and double.\n";
}


////////// TESTING GETVIEW //////////

TEST_F(ManagerTest, LoopViewChangeBasicDirectWithReference) {
    // making sure entities are created properly
    size_t created = fillRegistryBasic(mng);
    ASSERT_EQ(mng.Size(), created);

    std::map<vecs::Handle, double> origin;

    // change values directly
    for( auto [handle, d] : mng.GetView<vecs::Handle, double&>()) {
        origin.insert({handle, d.Value()});
        d += 1.0;
    }

    for( auto [h, d] : origin) {
        ASSERT_EQ(d+1.0, mng.Get<double>(h));
    }
}


TEST_F(ManagerTest, LoopViewChangeBasicDirectWithoutReference) {
    // making sure entities are created properly
    size_t created = fillRegistryBasic(mng);
    ASSERT_EQ(mng.Size(), created);

    std::map<vecs::Handle, float> origin;

    // change values directly via Get
    for( auto [handle, f] : mng.GetView<vecs::Handle, float>()) {
        origin.insert({handle, f});
        mng.Get<float&>(handle) += 1.0f;
    }

    for( auto [h, f] : origin) {
        ASSERT_EQ(f+1.0f, mng.Get<float>(h));
        //std::cout << "handle " << h << " old=" << f << ", new=" << mng.Get<float>(h);
    }
}


TEST_F(ManagerTest, LoopViewChangeComplexDirectWithReference) {
    // making sure entities are created properly
    size_t created = fillRegistryComplex(mng);
    ASSERT_EQ(mng.Size(), created);

    std::map<vecs::Handle, std::tuple<Position, Velocity>> origin;

    // change values directly
    for( auto [handle, p, v] : mng.GetView<vecs::Handle, Position&, Velocity&>()) {
        origin.insert({handle, std::tuple(p.Value(), v.Value())});
        p.Value().x += p.Value().x*v.Value().x;
        p.Value().y += p.Value().y*v.Value().y;
    }

    for( auto [h, tup] : origin) {
        ASSERT_EQ(std::get<Position>(tup).x + std::get<Position>(tup).x * std::get<Velocity>(tup).x, mng.Get<Position>(h).x);
        ASSERT_EQ(std::get<Position>(tup).y + std::get<Position>(tup).y * std::get<Velocity>(tup).y, mng.Get<Position>(h).y);
    }
}