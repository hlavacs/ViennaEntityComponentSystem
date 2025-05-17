#define REGISTRYTYPE_PARALLEL

#include <iostream>
#include <string>
#include <gtest/gtest.h>

#include "VECSManager.h"

class ManagerTest : public testing::Test {
    protected:
        ManagerTest(){}

        vecs::Manager mng;
};


TEST_F(ManagerTest, CreateAndChangeEntityWorks) {
    vecs::Handle h1 = mng.CreateEntity(5, 7.6f, 3.2);

    mng.waitIdle();
    ASSERT_EQ(mng.GetComponent<int>(h1), 5);
    ASSERT_EQ(mng.GetComponent<float>(h1), 7.6f);
    ASSERT_EQ(mng.GetComponent<double>(h1), 3.2);

    mng.waitIdle();
    auto [c1, c2] = mng.GetComponent<int&, double&>(h1);
    c1 = 7;
    c2 = 1.8;

    mng.waitIdle();
    ASSERT_EQ(mng.GetComponent<int>(h1), 7);
    ASSERT_EQ(mng.GetComponent<double>(h1), 1.8);
    mng.waitIdle();
}


TEST_F(ManagerTest, PutComponentWorks) {
    vecs::Handle h2 = mng.CreateEntity(5);
    mng.PutComponent(h2, 6.4);

    ASSERT_EQ(mng.GetComponent<double>(h2), 6.4);

    std::tuple<float, std::string> tup = {1.2f, "hi"};
    mng.PutComponent(h2, tup);

    ASSERT_EQ(mng.GetComponent<float>(h2), 1.2f);
    ASSERT_EQ(mng.GetComponent<std::string>(h2), "hi");
}



TEST_F(ManagerTest, AddTagsWorks) {
    vecs::Handle h3 = mng.CreateEntity(4, 5.5, 6.6f);
    vecs::Handle h4 = mng.CreateEntity(9, 8.8, 7.7f);
    vecs::Handle h5 = mng.CreateEntity(7, 6.6, 5.5f);

    mng.waitIdle();

    mng.AddTags(h3, 1ul, 3ul);
    mng.waitIdle();
    mng.AddTags(h4, 2ul, 3ul, 1ul);
    mng.waitIdle();
    mng.AddTags(h5, 1ul, 2ul);

    mng.waitIdle();

    int yesTagsCorrect = 0;
    int yesNoTagsCorrect = 0;

    auto yesTagsView = mng.template GetView<vecs::Handle>(std::vector<size_t>{1ul});
    auto yesNoTagsView = mng.template GetView<vecs::Handle>(std::vector<size_t>{2ul}, std::vector<size_t>{3ul});

    mng.waitIdle();
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
    vecs::Handle h6 = mng.CreateEntity(4, 5.5, 6.6f);
    mng.waitIdle();
    mng.AddTags(h6, 4ul, 5ul, 6ul);

    bool tagErased = true;
    mng.waitIdle();
    mng.EraseTags(h6, 6ul);
    mng.waitIdle();

    auto res = mng.template GetView<vecs::Handle>(std::vector<size_t>{6ul});
    for ( auto handle : res)  {
        if (handle == h6) tagErased = false;
    }

    ASSERT_TRUE(tagErased);
}


TEST_F(ManagerTest, EraseComponentAndEntityWorks) {
    vecs::Handle h7 = mng.CreateEntity(5);
    vecs::Handle h8 = mng.CreateEntity(4, 5.5f);
    mng.waitIdle();

    mng.EraseEntity(h7);
    mng.waitIdle();
    mng.EraseComponents<int>(h8);
    mng.waitIdle();

    bool noInt = true;
    auto res = mng.template GetView<vecs::Handle, int>();

    for ( auto [handle, i] : res ) {
        if (handle == h7 || handle == h8) noInt = false;
    }

    ASSERT_TRUE(noInt);
}


TEST_F(ManagerTest, ClearRegistryWorks) {
    vecs::Handle h9 = mng.CreateEntity(5);
    vecs::Handle h10 = mng.CreateEntity(4, 5.5f);
    mng.waitIdle();

    int size = 0;
    auto resa = mng.GetView<vecs::Handle>();
    
    for ( auto handle : resa) {
        size += 1;
    }

    ASSERT_EQ(size, 2);
    mng.waitIdle();

    mng.ClearRegistry();
    mng.waitIdle();
    
    size = 0;
    auto resb = mng.GetView<vecs::Handle>();

    for ( auto handle : resb ) {
        size += 1;
    }

    ASSERT_EQ(size, 0);
}