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

    ASSERT_EQ(mng.GetComponent<int>(h1), 5);
    ASSERT_EQ(mng.GetComponent<float>(h1), 7.6f);
    ASSERT_EQ(mng.GetComponent<double>(h1), 3.2);

    auto [c1, c2] = mng.GetComponent<int&, double&>(h1);
    c1 = 7;
    c2 = 1.8;

    ASSERT_EQ(mng.GetComponent<int>(h1), 7);
    ASSERT_EQ(mng.GetComponent<double>(h1), 1.8);
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

    mng.AddTags(h3, 1ul, 3ul);
    mng.AddTags(h4, 2ul, 3ul, 1ul);
    mng.AddTags(h5, 1ul, 2ul);

    int yesTagsCorrect = 0;
    int yesNoTagsCorrect = 0;

    for ( auto handle : mng.template GetView<vecs::Handle>(std::vector<size_t>{1ul}) ) {
        //expect h3, h4, h5
        if (handle == h3 || handle == h4 || handle == h5) yesTagsCorrect += 1;
    }

    for ( auto handle : mng.template GetView<vecs::Handle>(std::vector<size_t>{2ul}, std::vector<size_t>{3ul}) ) {
        //expect h5
        if (handle == h5) yesNoTagsCorrect += 1;
    }

    ASSERT_EQ(yesTagsCorrect, 3);
    ASSERT_EQ(yesNoTagsCorrect, 1);
}


TEST_F(ManagerTest, EraseTagsWorks) {
    vecs::Handle h1 = mng.CreateEntity(4, 5.5, 6.6f);

    mng.AddTags(h1, 1ul, 2ul, 3ul);

    bool tagErased = true;

    mng.EraseTags(h1, 3ul);
    for ( auto handle : mng.template GetView<vecs::Handle>(std::vector<size_t>{3ul}) ) {
        if (handle == h1) tagErased = false;
    }

    ASSERT_TRUE(tagErased);
}


TEST_F(ManagerTest, EraseComponentAndEntityWorks) {
    vecs::Handle h1 = mng.CreateEntity(5);
    vecs::Handle h2 = mng.CreateEntity(4, 5.5f);

    mng.EraseEntity(h1);
    mng.EraseComponents<int>(h2);
    bool noInt = true;

    for ( auto [handle, i] : mng.GetView<vecs::Handle, int>() ) {
        if (handle == h1 || handle == h2) noInt = false;
    }

    ASSERT_TRUE(noInt);
}


TEST_F(ManagerTest, ClearRegistryWorks) {
    vecs::Handle h1 = mng.CreateEntity(5);
    vecs::Handle h2 = mng.CreateEntity(4, 5.5f);

    int size = 0;
    for ( auto handle : mng.GetView<vecs::Handle>() ) {
        size += 1;
    }

    ASSERT_EQ(size, 2);

    mng.ClearRegistry();

    size = 0;
    for ( auto handle : mng.GetView<vecs::Handle>() ) {
        size += 1;
    }

    ASSERT_EQ(size, 0);
}