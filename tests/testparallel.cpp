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