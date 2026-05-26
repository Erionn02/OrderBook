#include <gtest/gtest.h>
#include "TestClass2.hpp"

struct ExampleUnitTest2 : public testing::Test {
    TestClass2 test;
};

TEST_F(ExampleUnitTest2, exampleTestTrue) {
    auto res = test.getBool(true);
    ASSERT_TRUE(res);
}

TEST_F(ExampleUnitTest2, exampleTestFalse) {
    ASSERT_FALSE(test.getBool(false));
}