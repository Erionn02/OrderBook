#include <gtest/gtest.h>

struct ExampleUnitTest : public testing::Test {
};

TEST_F(ExampleUnitTest, exampleTestTrue) {
    ASSERT_TRUE(true);
}

