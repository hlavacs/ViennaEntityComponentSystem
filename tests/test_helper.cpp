#include "VECSManager.h"

/// @brief Inserts entities of one, two or three components of int, double and float
size_t fillRegistryBasic(vecs::Manager& mng, int oneComp = 5, int twoComp = 5, int threeComp = 5) {
    std::vector<int> integers = {1, 2, 3, 4, 5};
    std::vector<double> doubles = {1.2, 2.3, 3.4, 4.5, 5.6};
    std::vector<float> floats = {1.1f, 2.2f, 3.3f, 4.4f, 5.5f};

    size_t totalNew = 0;

    for (size_t i = 0; i < oneComp; ++i) {
        mng.Insert(integers[i]);
        mng.Insert(doubles[i]);
        totalNew += 2;
    }

    for (size_t i = 0; i < twoComp; ++i) {
        mng.Insert(integers[i], doubles[i]);
        mng.Insert(integers[(i+1)%integers.size()], floats[i]);
        mng.Insert(doubles[(i+1)%doubles.size()], floats[(i+1)%floats.size()]);
        totalNew += 3;
    }

    for (size_t i = 0; i < threeComp; ++i) {
        mng.Insert(integers[i], doubles[i], floats[i]);
        totalNew += 1;
    }

    return totalNew;
}