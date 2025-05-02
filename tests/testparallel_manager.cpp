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
    vecs::Handle h1 = mng.createEntity(5, 7.6f, 3.2);

    ASSERT_EQ(mng.getComponent<int>(h1), 5);
    ASSERT_EQ(mng.getComponent<float>(h1), 7.6f);
    ASSERT_EQ(mng.getComponent<double>(h1), 3.2);

    auto [c1, c2] = mng.getComponent<int&, double&>(h1);
    c1 = 7;
    c2 = 1.8;

    ASSERT_EQ(mng.getComponent<int>(h1), 7);
    ASSERT_EQ(mng.getComponent<double>(h1), 1.8);
}


TEST_F(ManagerTest, PutComponentWorks) {
    vecs::Handle h2 = mng.createEntity(5);

    mng.putComponent(h2, 6.4);

    ASSERT_EQ(mng.getComponent<double>(h2), 6.4);


    std::tuple<float, std::string> tup = {1.2f, "hi"};
    mng.putComponent(h2, tup);

    ASSERT_EQ(mng.getComponent<float>(h2), 1.2f);
    ASSERT_EQ(mng.getComponent<std::string>(h2), "hi");
}


TEST_F(ManagerTest, AddTagsWorks) {
    vecs::Handle h3 = mng.createEntity(4, 5.5, 6.6f);
    vecs::Handle h4 = mng.createEntity(9, 8.8, 7.7f);
    vecs::Handle h5 = mng.createEntity(7, 6.6, 5.5f);

    mng.addTags(h3, 1ul, 3ul);
    mng.addTags(h4, 2ul, 3ul, 1ul);
    mng.addTags(h5, 1ul, 2ul);

    int yesTagsCorrect = 0;
    int yesNoTagsCorrect = 0;

    for ( auto handle : mng.template GetView<vecs::Handle>(std::vector<size_t>{1ul})) {
        //expect h3, h4, h5
        if (handle == h3 || handle == h4 || handle == h5) yesTagsCorrect += 1;
    }

    for ( auto handle : mng.template GetView<vecs::Handle>(std::vector<size_t>{2ul}, std::vector<size_t>{3ul})) {
        //expect h5
        if (handle == h5) yesNoTagsCorrect += 1;
    }

    ASSERT_EQ(yesTagsCorrect, 3);
    ASSERT_EQ(yesNoTagsCorrect, 1);
}


TEST_F(ManagerTest, EraseTagsWorks) {

}


TEST_F(ManagerTest, EraseComponentWorks) {

}


TEST_F(ManagerTest, ClearRegistryWorks) {

}