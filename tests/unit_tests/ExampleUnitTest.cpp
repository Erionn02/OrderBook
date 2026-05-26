#include <gtest/gtest.h>
#include "TestClass.hpp"

struct ExampleUnitTest : public testing::Test {
    TestClass test;
};

TEST_F(ExampleUnitTest, exampleTestTrue) {
    auto res = test.getBool(true);
    ASSERT_TRUE(res);
}

TEST_F(ExampleUnitTest, exampleTestFalse) {
    ASSERT_FALSE(test.getBool(false));
}