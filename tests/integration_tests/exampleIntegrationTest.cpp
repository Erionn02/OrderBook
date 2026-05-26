#include <gtest/gtest.h>
#include "TestClass.hpp"
#include "TestClass2.hpp"

TEST(exampleIntegrationTest, EmptyTest) {
    TestClass tc;
    TestClass2 tc2;
    ASSERT_EQ(tc.getBool(true), tc2.getBool(true));
}